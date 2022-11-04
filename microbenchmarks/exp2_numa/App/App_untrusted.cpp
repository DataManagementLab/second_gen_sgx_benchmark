#include <assert.h>
#include <stdlib.h>
#include <algorithm>
#include <atomic>
#include <cstring>  // for memset
#include <numeric>
#include <string>
#include <vector>
#include "../defs.h"
#include "PerfEvent.hpp"
#include "config.h"
#include "csvHelpers.h"

#define ENCLAVE_FILENAME "countertestenclave.signed.so"  // linux
//#define _T("Enclave.signed.dll") //windows

template <typename T>
inline void DO_NOT_OPTIMIZE(T const& value)
{
#if defined(_clang_)
   asm volatile("" : : "g"(value) : "memory");
#else
   asm volatile("" : : "i,r,m"(value) : "memory");
#endif
}

static __inline__ int64_t rdtsc(void)
{
   unsigned int lo, hi;
   __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
   return ((uint64_t)hi << 32) | lo;
}

struct Node {
   Node* next{nullptr};
   uint64_t data{0};     // just to ensure that it will not be outcompiled
   uint64_t padding[6];  // (64 -8-8)/8 (8 is lengtho of uint64)
};

Node* allocateList(uint64_t number_nodes)
{
   if (sizeof(Node) != 64) {
      throw std::logic_error("Node size wrong" + std::to_string(sizeof(Node)));
   }
   Node* node = new Node[number_nodes + 1];
   return node;
}

void freeList(Node* root)
{
   delete[] root;
}

uint64_t rnd(uint64_t& seed)
{
   uint64_t x = seed; /* state nicht mit 0 initialisieren */
   x ^= x >> 12;      // a
   x ^= x << 25;      // b
   x ^= x >> 27;      // c
   seed = x;
   return x * 0x2545F4914F6CDD1D;
}

void random_shuffle(std::vector<uint64_t>& nodes_idxs)
{
   auto n = nodes_idxs.size();
   uint64_t seed = 19650218ULL;
   for (uint64_t i = n - 1; i > 0; --i) {
      std::swap(nodes_idxs[i], nodes_idxs[rnd(seed) % (i + 1)]);
   }
}

void shuffleList(Node*& root, uint64_t number_nodes, bool shuffle)
{
   // -------------------------------------------------------------------------------------
   // create help vector of idx
   std::vector<uint64_t> node_idxs(number_nodes - 1);  // one less because we do not start at 0 -> root should stay root
   std::iota(std::begin(node_idxs), std::end(node_idxs), 1);
   if (shuffle)
      random_shuffle(node_idxs);
   // -------------------------------------------------------------------------------------
   uint64_t counter = 0;
   auto* prev = root;
   for (auto& idx : node_idxs) {
      prev->next = &root[idx];
      prev->data = counter++;
      prev = prev->next;
   }
}

void untrusted_main(uint64_t* number_nodes,
                    uint64_t* prealloc,
                    uint64_t* result,
                    uint64_t* cycles,
                    uint64_t* num_measurements,
                    uint8_t shuffle)
{
   // -------------------------------------------------------------------------------------
   std::cout << "Preallocating" << std::endl;
   auto palloc = new char[*prealloc];
   memset(palloc, 0, *prealloc);
   std::cout << "allocating List" << std::endl;
   auto root = allocateList(*number_nodes);
   auto node = root;

   uint64_t n = ((uintptr_t)node) % 64;
   if (n != 0) {
      char* p = (char*)node;
      p = p + (64 - n);
      node = (Node*)p;
   }
   if (((uintptr_t)node) % 64 != 0) {
      throw std::logic_error("Not CL aligned " + std::to_string(((uintptr_t)node) % 64));
   }
   *result = 0;
   shuffleList(node, *number_nodes, FLAGS_SHUFFLE);
   // iterate
   std::cout << "Iteration starts!" << std::endl;
   auto* current = node;
   uint64_t counter = 0;
   uint64_t measurements = 0;
   std::atomic<uint64_t> cb = rdtsc();
   std::atomic<uint64_t> ce;
   while (current) {
      if (counter % OFFSET == 0 && measurements < NUM_LATENCIES) {
         ce = rdtsc();
         uint64_t duration = ce.load() - cb.load();
         cycles[measurements] = duration;
         cb.store(ce.load());
         measurements++;
      }
      *result += current->data;
      current = current->next;
      counter++;
   }
   *num_measurements = measurements;
   freeList(root);
   delete palloc;
}

int main(int argc, char* argv[])
{
   uint64_t clocks_before = 0;
   uint64_t clocks_after = 0;
   uint64_t result = 0;
   // -------------------------------------------------------------------------------------
   gflags::SetUsageMessage("Internal hash table benchmark");
   gflags::ParseCommandLineFlags(&argc, &argv, true);
   // -------------------------------------------------------------------------------------
   uint64_t number_nodes = (FLAGS_SIZE_MB * 1e6) / sizeof(Node);
   uint64_t prealloc = (FLAGS_PREALLOCATE * 1e6);
   BenchmarkParameters params;
   params.setParam("name", "Memory Latency");
   params.setParam("dataSize", FLAGS_SIZE_MB);
   char* original_root;
   char* pointer_to_root;
   std::cout << "Starting measurement" << std::endl;

   std::atomic<uint64_t> cb;
   std::atomic<uint64_t> ce;
   uint64_t* cycles = new uint64_t[NUM_LATENCIES];
   uint64_t num_measurements = 0;
   {
#if VTUNE == 0
      PerfEventBlock e(1, params, true);
#endif
      cb = rdtsc();
      untrusted_main(&number_nodes, &prealloc, &result, cycles, &num_measurements, FLAGS_SHUFFLE);
      ce = rdtsc();
   }
   std::sort(cycles, cycles + num_measurements);

   std::vector<double> nanoseconds(num_measurements);
   std::transform(cycles, cycles + num_measurements, nanoseconds.begin(),
                  [](uint64_t cycles) -> double { return (cycles) / (double)FLAGS_GHZ / OFFSET; });

   // median
   uint64_t measurements = num_measurements;
   double min = nanoseconds[0];
   double max = nanoseconds.back();
   double p99 = nanoseconds[num_measurements * 0.99];
   double p999 = nanoseconds[num_measurements * 0.999];
   double avg = std::accumulate(nanoseconds.begin(), nanoseconds.end(), 0.0) / nanoseconds.size();
   double median = nanoseconds[num_measurements * 0.5];
   // n-smallest latencies
   std::string lowestN = "";
   for (size_t i = 0; i < TOP_N - 1; i++) {
      lowestN += std::to_string(nanoseconds[i]) + ";";
   }
   lowestN += std::to_string(nanoseconds[TOP_N - 1]);

   // n-largest latencies
   std::string largestN = "";
   for (size_t i = num_measurements - TOP_N; i < num_measurements - 1; i++) {
      largestN += std::to_string(nanoseconds[i]) + ";";
   }
   largestN += std::to_string(nanoseconds[num_measurements - 1]);

   std::cout << "result " << result << std::endl;
   std::vector<std::string> header = {
       "mode", "number_nodes", "size_mb", "shuffle", "prealloc_mb", "cross_numa", "total_cycles", "duration_in_nsec",
       //   "scan_cycles",
       //   "scan_duration_in_nsec",
       //   "relative_cycles_scan",
       //   "relative_time_scan",
       "measurements", "min", "max", "99%", "99.9%", "average", "median", "lowestN", "largestN", "GHz"};
   auto diff = ce.load() - cb.load();
   auto t = diff / FLAGS_GHZ;
   // auto diff_scan = clocks_after - clocks_before;
   // auto t_scan = diff_scan / FLAGS_GHZ;
   // auto rel_cycles_scan = diff_scan / (double)number_nodes;
   // auto rel_t_scan = rel_cycles_scan / FLAGS_GHZ;
   std::vector<std::string> values = {
       "untrusted", std::to_string(number_nodes), std::to_string(FLAGS_SIZE_MB),
       (FLAGS_SHUFFLE ? "random" : "sequential"), std::to_string(FLAGS_PREALLOCATE),
       (FLAGS_PREALLOCATE > 0 ? "crossNuma" : "localNuma"), std::to_string(diff), std::to_string(t),
       //   std::to_string(diff_scan),
       //   std::to_string(t_scan),
       //   std::to_string(rel_cycles_scan),
       //   std::to_string(rel_t_scan),
       std::to_string(measurements), std::to_string(min), std::to_string(max), std::to_string(p99),
       std::to_string(p999), std::to_string(avg), std::to_string(median), lowestN, largestN, std::to_string(FLAGS_GHZ)};
   std::ofstream csv_file;
   openCSV(csv_file, FLAGS_csvFile, header);
   logToCSV(csv_file, values);
}