#include <algorithm>
#include <atomic>
#include <chrono>
#include <numeric>
#include <thread>
#include "../defs.h"
#include "Enclave_u.h"
#include "PerfEvent.hpp"
#include "config.h"
#include "csvHelpers.h"
#include "sgxerrors.h"

#define ENCLAVE_FILENAME "exp3_pagingenclave.signed.so"  // linux
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

int main(int argc, char* argv[])
{
   // -------------------------------------------------------------------------------------
   gflags::SetUsageMessage("Internal hash table benchmark");
   gflags::ParseCommandLineFlags(&argc, &argv, true);
   // -------------------------------------------------------------------------------------
   // Output warning if enclave size will cause paging during enclave creation
   uint64_t total_max_epc_byte = FLAGS_EPC_MAX_MB * FLAGS_NUM_NUMA_NODES * 1e6;
   bool ENCLAVE_CREATION_PAGING = false;
   if (total_max_epc_byte < FLAGS_ENC_MAXHEAP_B) {
      std::cout << "WARNING: total max epc is " << total_max_epc_byte << " < " << FLAGS_ENC_MAXHEAP_B
                << " enclave heap (both bytes), ie. paging will take place during enclave creation" << std::endl;
      ENCLAVE_CREATION_PAGING = true;
   }

   // -------------------------------------------------------------------------------------
   // Compute number of nodes based on MB input
   uint64_t data_size_byte = (FLAGS_MULTIPLE_EPC_TOTAL * total_max_epc_byte * 0.95);
   uint64_t number_nodes = (data_size_byte) / sizeof(Node);
   std::cout << "Experiment with " << data_size_byte << " byte of data, these are so many 4KB pages: " << number_nodes
             << std::endl;

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

   std::cout << "Starting experiment" << std::endl;

   std::atomic<uint64_t> cb;
   std::atomic<uint64_t> ce;
   uint64_t* cycles = new uint64_t[NUM_LATENCIES];
   uint64_t num_measurements = 0;
   std::chrono::time_point<std::chrono::steady_clock> start_time = std::chrono::steady_clock::now();
   {
      cb = rdtsc();
      ecall_main(global_eid, &number_nodes, &result, cycles, &num_measurements);
      ce = rdtsc();
   }
   std::chrono::time_point<std::chrono::steady_clock> end_time = std::chrono::steady_clock::now();

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

   std::vector<std::string> stats_header = {"min", "max", "99%", "99.9%", "average", "median"};
   for (auto&& head : stats_header) {
      std::cout << head << ",";
   }
   std::cout << std::endl;

   std::vector<std::string> stats = {std::to_string(min),  std::to_string(max), std::to_string(p99),
                                     std::to_string(p999), std::to_string(avg), std::to_string(median)};
   for (auto&& stat : stats) {
      std::cout << stat << ",";
   }
   std::cout << std::endl;

   std::cout << "result " << result << std::endl;
   std::vector<std::string> header = {"mode", "number_nodes", "size_mb", "ratio_data_EPC_TOTAL", "multiple_total_epc",
                                      "total_cycles", "cycles_duration_nsec", "chrono_duration_nsec", "measurements",
                                      "cycles_per_offset", "measurement_offset", "avg_cycles_offset", "avg_nsec_offset",
                                      // default headers that describe system config
                                      "GHz", "EPC_MAX_MB", "NUM_NUMA_NODES", "EPC_MAX_TOTAL_MB", "ENC_MAXHEAP_B"};
   auto diff = ce.load() - cb.load();
   auto t = diff / FLAGS_GHZ;
   std::chrono::duration<double, std::nano> exp_duration = (end_time - start_time);

   std::ofstream csv_file;
   openCSV(csv_file, FLAGS_csvFile, header);
   for (uint64_t i = 0; i < num_measurements; i++) {
      uint64_t latency_cycles = cycles[i];
      std::vector<std::string> values = {
          "trusted", std::to_string(number_nodes), std::to_string((data_size_byte / 1e6)),
          (std::to_string(FLAGS_MULTIPLE_EPC_TOTAL) + ":1"), std::to_string(FLAGS_MULTIPLE_EPC_TOTAL),
          std::to_string(diff), std::to_string(t), std::to_string(exp_duration.count()),
          std::to_string(num_measurements), std::to_string(latency_cycles), std::to_string(OFFSET),
          std::to_string(latency_cycles / (double)OFFSET),
          std::to_string(latency_cycles / (double)FLAGS_GHZ / (double)OFFSET),
          // default headers that describe system config
          std::to_string(FLAGS_GHZ), std::to_string(FLAGS_EPC_MAX_MB), std::to_string(FLAGS_NUM_NUMA_NODES),
          std::to_string(FLAGS_EPC_MAX_MB * FLAGS_NUM_NUMA_NODES), std::to_string(FLAGS_ENC_MAXHEAP_B)};
      if (values.size() != header.size())
         throw std::runtime_error("Illegal csv format");
      logToCSV(csv_file, values);
   }

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

void ocall_sleep_sec(uint64_t sec)
{
   std::this_thread::sleep_for(std::chrono::seconds(sec));
}
