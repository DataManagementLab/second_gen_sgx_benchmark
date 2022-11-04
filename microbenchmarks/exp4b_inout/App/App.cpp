#include <algorithm>
#include <atomic>
#include <chrono>
#include <numeric>
#include <thread>
#include <vector>
#include "../defs.h"
#include "Enclave_u.h"
#include "PerfEvent.hpp"
#include "config.h"
#include "csvHelpers.h"
#include "sgxerrors.h"

#define ENCLAVE_FILENAME "exp4b_inoutenclave.signed.so"  // linux
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
   // Parse Commandline arguments
   gflags::SetUsageMessage("SGX exp4b_inout benchmark");
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
   // Load Enclave file
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

   // -------------------------------------------------------------------------------------
   // Do setup

   // allocate data to process in enclave
   uint64_t data_size_kib = FLAGS_DATA_SIZE_MiB * 1024;
   uint64_t data_length = (FLAGS_DATA_SIZE_MiB * 1024 * 1024) / sizeof(uint64_t);
   std::vector<uint64_t> table(data_length);
   std::iota(std::begin(table), std::end(table), 0);  // fill vector with range starting from 0

   std::cout << "Check result " << ((data_length - 1) * (data_length - 1) + (data_length - 1)) / 2
             << "Not applicable if modulo_param > 1, modulo_param is " << FLAGS_MODULO_PARAM << std::endl;

   std::vector<uint64_t> batch_sizes_kib;
   uint64_t batch_size_kib = 4;
   std::cout << "The following batch sizes (in KiB) will be used for the experiment:\n";
   while (batch_size_kib <= data_size_kib) {
      std::cout << batch_size_kib << ", ";
      batch_sizes_kib.push_back(batch_size_kib);
      batch_size_kib = batch_size_kib * 2;
   }
   std::cout << std::endl;

   std::vector<std::string> header = {
       "ts",  "ops",        "num_ecalls",     "sgx_version",      "data_size_mib", "batch_size_kib",
       "GHz", "EPC_MAX_MB", "NUM_NUMA_NODES", "EPC_MAX_TOTAL_MB", "ENC_MAXHEAP_B"};
   std::vector<std::string> values = {
       "0", "0", "0", FLAGS_SGX_VERSION, std::to_string(FLAGS_DATA_SIZE_MiB), std::to_string(batch_size_kib),
       // default headers that describe system config
       std::to_string(FLAGS_GHZ), std::to_string(FLAGS_EPC_MAX_MB), std::to_string(FLAGS_NUM_NUMA_NODES),
       std::to_string(FLAGS_EPC_MAX_MB * FLAGS_NUM_NUMA_NODES), std::to_string(FLAGS_ENC_MAXHEAP_B)};
   std::ofstream csv_file;
   openCSV(csv_file, FLAGS_csvFile, header);

   // performance counter
   uint64_t ops = 0;
   uint64_t ecall_counter = 0;
   uint64_t time = 0;
   uint8_t pause_measurement = 0;  // 0 pausse measuring thread

   // measuring thread
   std::atomic<bool> stop_thread{false};
   std::thread t1([&]() {
      uint64_t time = 0;

      volatile uint64_t* ops_v = &ops;
      volatile uint64_t* ecalls_v = &ecall_counter;
      uint64_t ops_old = 0;
      uint64_t ecalls_old = 0;

      volatile uint64_t* batch_size_kib_v = &batch_size_kib;
      volatile uint8_t* pause_measurement_v = &pause_measurement;

      while (!stop_thread) {
         if ((*pause_measurement_v) == 1) {
            continue;  // poor's man barrier
         }

         time++;
         values[0] = std::to_string(time);
         values[1] = std::to_string(*ops_v - ops_old);
         values[2] = std::to_string(*ecalls_v - ecalls_old);
         values[5] = std::to_string(*batch_size_kib_v);
         logToCSV(csv_file, values);

         ops_old = *ops_v;
         ecalls_old = *ecalls_v;

         sleep(1);
      }
   });

   // iterate over experiments
   if (FLAGS_REVERSE_BATCH_ORDER) {
      std::reverse(batch_sizes_kib.begin(), batch_sizes_kib.end());
   }
   uint64_t modulo_param = FLAGS_MODULO_PARAM;
   for (size_t i = 0; i < batch_sizes_kib.size(); i++) {
      uint64_t result = 0;  // result of computation
      batch_size_kib = batch_sizes_kib[i];
      uint64_t scan_length = batch_size_kib * 1024 / sizeof(uint64_t);
      uint64_t current_batch = 0;
      // scan data
      while (current_batch + scan_length <= data_length) {
         ecall_main(global_eid, &table[current_batch], scan_length, &ops, &ecall_counter, &modulo_param, &result);
         current_batch += scan_length;
      }
      if (current_batch < data_length) {
         // scan last few items
         ecall_main(global_eid, &table[current_batch], (data_length - current_batch), &ops, &ecall_counter,
                    &modulo_param, &result);
      }
      std::cout << "Result for batch size " << batch_size_kib << " data length " << data_length << " is:\t " << result
                << std::endl;
   }

   stop_thread = true;
   t1.join();

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
