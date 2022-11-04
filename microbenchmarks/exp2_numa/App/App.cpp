#include <algorithm>
#include <atomic>
#include <numeric>
#include "../defs.h"
#include "Enclave_u.h"
#include "PerfEvent.hpp"
#include "config.h"
#include "csvHelpers.h"
#include "sgxerrors.h"

#define ENCLAVE_FILENAME "numatestenclave.signed.so"  // linux
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

sgx_enclave_id_t global_eid = 0;

/* Initialize the enclave:
 *   Call sgx_create_enclave to initialize an enclave instance
 */
int initialize_enclave(const char* enclave_file)
{
   sgx_status_t ret = SGX_ERROR_UNEXPECTED;

   /* Call sgx_create_enclave to initialize an enclave instance */
   /* Debug Support: set 2nd parameter to 1 */
   ret = sgx_create_enclave(enclave_file, SGX_DEBUG_FLAG, NULL, NULL, &global_eid, NULL);
   if (ret != SGX_SUCCESS) {
      print_error_message(ret);
      return -1;
   }

   return 0;
}

/* Destroy the enclave:
 *   Call sgx_destroy_enclave to destroy the enclave instance
 */
int destroy_enclave()
{
   sgx_status_t ret = SGX_ERROR_UNEXPECTED;

   /* Call sgx_destroy_enclave to destroy  the enclave instance */
   ret = sgx_destroy_enclave(global_eid);
   if (ret != SGX_SUCCESS) {
      print_error_message(ret);
      return -1;
   }

   return 0;
}

static __inline__ int64_t rdtsc_s(void)
{
   unsigned a, d;
   asm volatile("cpuid" ::: "%rax", "%rbx", "%rcx", "%rdx");
   asm volatile("rdtsc" : "=a"(a), "=d"(d));
   return ((unsigned long)a) | (((unsigned long)d) << 32);
}

static __inline__ int64_t rdtsc_e(void)
{
   unsigned a, d;
   asm volatile("rdtscp" : "=a"(a), "=d"(d));
   asm volatile("cpuid" ::: "%rax", "%rbx", "%rcx", "%rdx");
   return ((unsigned long)a) | (((unsigned long)d) << 32);
}

struct alignas(64) Node {
   Node* next{nullptr};
   uint64_t data{0};  // just to ensure that it will not be outcompiled
};

int main(int argc, char* argv[])
{
   // -------------------------------------------------------------------------------------
   gflags::SetUsageMessage("Internal hash table benchmark");
   gflags::ParseCommandLineFlags(&argc, &argv, true);
   // -------------------------------------------------------------------------------------
   std::string argv_str(argv[0]);
   std::string base = argv_str.substr(0, argv_str.find_last_of("/"));
   std::string enclave_file = base + "/" + ENCLAVE_FILENAME;
   const char* ENCLAVE_FILE = enclave_file.c_str();
   std::cout << "Enclave file is " << enclave_file << std::endl;
   // create enclave
   int init_enclave_result = 0;
   {
      std::cout << "Creating enclave" << std::endl;
      init_enclave_result = initialize_enclave(ENCLAVE_FILE);
      std::cout << "Creation done" << std::endl;
   }

   uint64_t clocks_before = 0;
   uint64_t clocks_after = 0;
   uint64_t result = 0;
   uint8_t shuffle = FLAGS_SHUFFLE;
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
      ecall_main(global_eid, &number_nodes, &prealloc, &result, cycles, &num_measurements, shuffle);
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
       //    "scan_cycles",
       //    "scan_duration_in_nsec",
       //    "relative_cycles_scan",
       //    "relative_time_scan",
       "measurements", "min", "max", "99%", "99.9%", "average", "median", "lowestN", "largestN", "GHz"};
   auto diff = ce.load() - cb.load();
   auto t = diff / FLAGS_GHZ;
   // auto diff_scan = clocks_after - clocks_before;
   // auto t_scan = diff_scan / FLAGS_GHZ;
   // auto rel_cycles_scan = diff_scan / (double)number_nodes;
   // auto rel_t_scan = rel_cycles_scan / FLAGS_GHZ;
   std::vector<std::string> values = {
       "trusted", std::to_string(number_nodes), std::to_string(FLAGS_SIZE_MB),
       (FLAGS_SHUFFLE ? "random" : "sequential"), std::to_string(FLAGS_PREALLOCATE),
       (FLAGS_PREALLOCATE > 0 ? "crossNuma" : "localNuma"), std::to_string(diff), std::to_string(t),
       //    std::to_string(diff_scan),
       //    std::to_string(t_scan),
       //    std::to_string(rel_cycles_scan),
       //    std::to_string(rel_t_scan),
       std::to_string(measurements), std::to_string(min), std::to_string(max), std::to_string(p99),
       std::to_string(p999), std::to_string(avg), std::to_string(median), lowestN, largestN, std::to_string(FLAGS_GHZ)};
   std::ofstream csv_file;
   openCSV(csv_file, FLAGS_csvFile, header);
   logToCSV(csv_file, values);

   destroy_enclave();
}

/* OCall functions */
void ocall_print_string(const char* str)
{
   /* Proxy/Bridge will check the length and null-terminate
    * the input string to prevent buffer overflow.
    */
   printf("%s\n", str);
   fflush(stdout);  // Will now print everything in the stdout buffer
}
