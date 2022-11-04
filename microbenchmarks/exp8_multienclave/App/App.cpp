#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <numeric>
#include <system_error>  // for std::error_code
#include <thread>
#include <vector>
#include "Enclave_u.h"
#include "config.h"
#include "csvHelpers.h"
#include "sgxerrors.h"

#define ENCLAVE_FILENAME "exp8_multienclaveenclave.signed.so"  // linux
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

/* Initialize the enclave:
 *   Call sgx_create_enclave to initialize an enclave instance
 */
int initialize_enclave(const char* enclave_file, sgx_enclave_id_t& enclave_id)
{
   sgx_status_t ret = SGX_ERROR_UNEXPECTED;

   /* Call sgx_create_enclave to initialize an enclave instance */
   /* Debug Support: set 2nd parameter to 1 */
   ret = sgx_create_enclave(enclave_file, SGX_DEBUG_FLAG, NULL, NULL, &enclave_id, NULL);
   if (ret != SGX_SUCCESS) {
      print_error_message(ret);
      return -1;
   }

   return 0;
}

/* Destroy the enclave:
 *   Call sgx_destroy_enclave to destroy the enclave instance
 */
int destroy_enclave(sgx_enclave_id_t enclave_id)
{
   sgx_status_t ret = SGX_ERROR_UNEXPECTED;

   /* Call sgx_destroy_enclave to destroy  the enclave instance */
   ret = sgx_destroy_enclave(enclave_id);
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

void createFile(uint64_t enclave_id)
{
   std::string path = "/tmp/e" + std::to_string(enclave_id) + ".txt";
   std::ofstream ofs(path);
   ofs << "1";
   ofs.close();
}

void waitForFile(std::string path)
{
   bool fileExists = std::filesystem::exists(path);
   while (!fileExists) {
      fileExists = std::filesystem::exists(path);
      continue;
   }
}

int main(int argc, char* argv[])
{
   // -------------------------------------------------------------------------------------
   // Parse Commandline arguments
   gflags::SetUsageMessage("SGX exp8_multienclave benchmark");
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

   // this will be updated by the enclave creation
   sgx_enclave_id_t global_eid = 0;

   std::string create_csv_path = "e" + std::to_string(FLAGS_ENCLAVE_ID) + "_creation.csv";
   std::ofstream csv_file;
   openCSV(csv_file, create_csv_path, {"creation_time", "heap_size"});

   std::cout << "Performing Creation..." << std::endl;
   // -------------------------------------------------------------------------------------
   // Load Enclave file
   std::string argv_str(argv[0]);
   std::string base = argv_str.substr(0, argv_str.find_last_of("/"));
   std::string enclave_file = base + "/" + ENCLAVE_FILENAME;
   const char* ENCLAVE_FILE = enclave_file.c_str();
   std::cout << "Enclave file is " << enclave_file << std::endl;
   // create enclave
   int init_enclave_result = 0;
   // note this might change the global_id depending on the deployed enclaves
   init_enclave_result = initialize_enclave(ENCLAVE_FILE, global_eid);
   auto startTime = std::chrono::steady_clock::now();
   createFile(FLAGS_ENCLAVE_ID);
   auto stopTime = std::chrono::steady_clock::now();
   auto duration = std::chrono::duration<double>(stopTime - startTime).count();
   std::cout << "Created enclave " << global_eid << " ENCLAVE_ID of experiment is " << FLAGS_ENCLAVE_ID << std::endl;
   logToCSV(csv_file, {std::to_string(duration), std::to_string(FLAGS_ENC_MAXHEAP_B)});

   // wait for experiment to start
   std::cout << "Wait for EXPERIMENT to start..." << std::endl;
   waitForFile("/tmp/start_experiment.txt");
   std::cout << "Starting experiment ..." << std::endl;

   uint64_t ops = 0;
   uint64_t page_count = 0;
   uint64_t inserts = 0;
   uint64_t lookups = 0;

   std::atomic<bool> stop_thread{false};
   std::thread t1([&]() {
      std::ofstream csv_file;
      std::ofstream::openmode open_flags = std::ios::app;
      std::string csv_path = "e" + std::to_string(FLAGS_ENCLAVE_ID) + "_btree_measurements.csv";
      csv_file.open(csv_path, open_flags);

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
   std::cout << "Btree ops" << ops << std::endl;

   createFile(FLAGS_ENCLAVE_ID);
   std::cout << "Experiment done!" << std::endl;

   std::cout << "Done" << std::endl;
   destroy_enclave(global_eid);
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

void ocall_print_number(uint64_t* number)
{
   /* Proxy/Bridge will check the length and null-terminate
    * the input string to prevent buffer overflow.

    */
   std::cout << "counter " << *number << std::endl;
}
