#include <inttypes.h>
#include <limits.h>
#include <unistd.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include "Enclave_u.h"
#include "sgxerrors.h"

#define ENCLAVE_FILENAME "exp1_btreeenclave.signed.so"  // linux

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

   uint64_t ops = 0;
   uint64_t page_count = 0;
   uint64_t inserts = 0;
   uint64_t lookups = 0;

   std::atomic<bool> stop_thread{false};
   char hostname[HOST_NAME_MAX];
   gethostname(hostname, HOST_NAME_MAX);
   std::string host(hostname);
   std::thread t1([&]() {
      std::ofstream csv_file;
      std::ofstream::openmode open_flags = std::ios::app;
      csv_file.open(host + "_b_tree_measurements.csv", open_flags);

      // print header
      csv_file << "ts, ops, inserts, lookups, tree_size_gb" << std::endl;

      uint64_t time = 0;

      volatile uint64_t* ops_v = &ops;
      volatile uint64_t* page_count_v = &page_count;
      volatile uint64_t* inserts_v = &inserts;
      volatile uint64_t* lookups_v = &lookups;

      uint64_t ops_old = 0;
      uint64_t inserts_old = 0;
      uint64_t lookups_old = 0;
      while (!stop_thread) {
         csv_file << time++ << "," << *ops_v - ops_old << "," << *inserts_v - inserts_old << ","
                  << *lookups_v - lookups_old << "," << (page_count * 4096) / (double)1e9 << std::endl;

         ops_old = *ops_v;
         inserts_old = *inserts_v;
         lookups_old = *lookups_v;

         sleep(1);
      }
   });

   // ecall
   ecall_run(global_eid, &ops, &inserts, &lookups, &page_count);
   stop_thread = true;
   t1.join();
   std::cout << "Btree ops " << ops << std::endl;
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
