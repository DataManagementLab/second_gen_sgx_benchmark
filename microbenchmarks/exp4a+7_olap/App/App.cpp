#include <inttypes.h>
#include <limits.h>
#include <unistd.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include "Enclave_u.h"
#include "PerfEvent.hpp"
#include "config.h"
#include "sgxerrors.h"

#define ENCLAVE_FILENAME "exp4a_olapenclave.signed.so"  // linux
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
   gflags::SetUsageMessage("Exp4a and Exp7 OLAP");
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
      // to measure creation
      std::ofstream csv_file;
      std::ofstream::openmode open_flags = std::ios::app;
      const std::string file_name = "creation_time_measurements.csv";
      bool csv_initialized = std::filesystem::exists(file_name);
      // open file
      csv_file.open(file_name, open_flags);

      // print header
      if (!csv_initialized) {
         csv_file << "creation_time, heap_size" << std::endl;
      }

      // measure creation time and output
      std::cout << "Creating enclave" << std::endl;
      auto startTime = std::chrono::steady_clock::now();
      init_enclave_result = initialize_enclave(ENCLAVE_FILE);
      auto stopTime = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration<double>(stopTime - startTime).count();
      std::cout << "Creation done in " << duration << " s" << std::endl;
      csv_file << duration << ", " << FLAGS_HEAP_SIZE << std::endl;
   }

   for (uint64_t seq = 0; seq <= 1; seq++) {
      uint64_t number_values = (FLAGS_DS_MB * 1e6) / sizeof(uint64_t);
      uint8_t sequential = seq;  // 1 is seq (true)
      // -----------------------------------------------------------------------------------
      // performance counter
      uint64_t ops = 0;
      uint64_t page_count = 0;
      uint64_t inserts = 0;
      uint64_t lookups = 0;

      uint8_t initialized = 0;  // 0 wait until table is loaded (fals)
      uint8_t running = 1;      // 1 is running (true)

      std::atomic<bool> stop_thread{false};
      char hostname[HOST_NAME_MAX];
      gethostname(hostname, HOST_NAME_MAX);
      std::string host(hostname);
      std::thread t1([&]() {
         std::ofstream csv_file;
         std::ofstream::openmode open_flags = std::ios::app;
         const std::string file_name = host + "_olap_measurements.csv";
         bool csv_initialized = std::filesystem::exists(file_name);
         // open file
         csv_file.open(file_name, open_flags);

         // print header
         if (!csv_initialized) {
            csv_file << "ts, ops, data_size, number_values, heap_size, order" << std::endl;
         }

         uint64_t time = 0;

         volatile uint64_t* ops_v = &ops;
         volatile uint64_t* page_count_v = &page_count;
         volatile uint64_t* inserts_v = &inserts;
         volatile uint64_t* lookups_v = &lookups;

         volatile uint8_t* initialized_v = &initialized;

         volatile uint8_t* running_v = &running;

         uint64_t ops_old = 0;
         uint64_t inserts_old = 0;
         uint64_t lookups_old = 0;
         while (!stop_thread) {
            if ((*initialized_v) == 0) {
               continue;  // poor's man barrier
            }

            csv_file << time++ << "," << *ops_v - ops_old << "," << FLAGS_DS_MB << "," << number_values << ","
                     << FLAGS_HEAP_SIZE << "," << ((sequential == 1) ? "SEQ" : "RND") << std::endl;

            std::cout << time << "," << *ops_v - ops_old << "," << FLAGS_DS_MB << "," << number_values << ","
                      << FLAGS_HEAP_SIZE << "," << ((sequential == 1) ? "SEQ" : "RND") << std::endl;

            ops_old = *ops_v;
            inserts_old = *inserts_v;
            lookups_old = *lookups_v;

            if (time >= FLAGS_SECONDS)
               *running_v = 0;

            sleep(1);
         }
      });

      // ecall
      ecall_run(global_eid, &ops, &running, &number_values, &sequential, &initialized);
      stop_thread = true;
      t1.join();
      std::cout << "OLAP scan ops" << ops << std::endl;
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

/* OCall functions */
void ocall_print_number(uint64_t* number)
{
   /* Proxy/Bridge will check the length and null-terminate
    * the input string to prevent buffer overflow.

    */
   std::cout << "enclave number " << *number << std::endl;
}
