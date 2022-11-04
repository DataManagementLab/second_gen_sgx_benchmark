#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include "../Defs.hpp"
#include "Enclave_u.h"
#include "PerfEvent.hpp"
#include "config.h"
#include "sgxerrors.h"

#define ENCLAVE_FILENAME "ycsbenclave.signed.so"  // linux

sgx_enclave_id_t global_eid = 0;

uint64_t wal_buffer_size = 0;
uint64_t wal_offset = 0;
uint64_t wal_buffer_idx = 0;
char* wal_buffer = nullptr;
char* wal_buffer_aligned = nullptr;
std::string path = "./ycsb_wal_trusted.wal";
int ssd_fd = -1;

int init_wal()
{
   path = FLAGS_YCSB_SEAL ? path += "_seal" : path;
   // make sure that wal file exists and is empty
   std::ofstream wal_file;
   std::ofstream::openmode open_flags = std::ios::app;
   wal_file.open(path, std::ios::trunc);  // this overwrite any existing file
   wal_file.close();
   struct stat fstat;
   stat(path.c_str(), &fstat);
   int blksize = (int)fstat.st_blksize;
   int align = blksize - 1;

   wal_buffer_size = blksize;

   int flags = O_RDWR | O_DIRECT;
   ssd_fd = open(path.c_str(), flags, 0666);
   if (ssd_fd == -1) {
      printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
   }
   wal_buffer = new char[((int)blksize + align)];
   wal_buffer_aligned = (char*)(((uintptr_t)wal_buffer + align) & ~((uintptr_t)align));
   return 0;
}

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

int main(int argc, char* argv[])
{
   // -------------------------------------------------------------------------------------
   gflags::SetUsageMessage("YCSB enclave");
   gflags::ParseCommandLineFlags(&argc, &argv, true);
   // -------------------------------------------------------------------------------------

   init_wal();

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

   uint64_t page_count = 0;
   {
      std::cout << "B-Tree creation ";
      ecall_create(global_eid, FLAGS_YCSB_records, &page_count, FLAGS_YCSB_WAL);
      std::cout << ((page_count)*ycsb::pageSize) / (double)1e9 << "  [OK ]" << std::endl;
   }

   std::vector<uint64_t> ratios = {5, 50, 95, 100};

   for (auto& read_ratio : ratios) {
      uint64_t ops = 0;
      uint64_t inserts = 0;
      uint64_t lookups = 0;
      uint8_t running = 1;  // 1 is running (true)

      std::atomic<bool> stop_thread{false};
      std::thread t1([&]() {
         std::ofstream csv_file;
         std::ofstream::openmode open_flags = std::ios::app;

         std::string file_name = "ycsb_trusted";
         file_name = FLAGS_YCSB_WAL ? file_name += "_wal" : file_name;
         file_name = FLAGS_YCSB_SEAL ? file_name += "_seal" : file_name;
         file_name += ".csv";
         bool csv_initialized = std::filesystem::exists(file_name);
         csv_file.open(file_name, open_flags);
         // print header
         if (!csv_initialized) {
            csv_file << "ts, readratio, wal, ops, inserts, lookups, tree_size_gb" << std::endl;
         }

         uint64_t time = 0;

         volatile uint64_t* ops_v = &ops;
         volatile uint64_t* page_count_v = &page_count;
         volatile uint64_t* inserts_v = &inserts;
         volatile uint64_t* lookups_v = &lookups;

         volatile uint8_t* running_v = &running;

         uint64_t ops_old = 0;
         uint64_t inserts_old = 0;
         uint64_t lookups_old = 0;
         while (!stop_thread) {
            csv_file << time++ << "," << read_ratio << "," << (FLAGS_YCSB_WAL ? "true" : "false") << ","
                     << *ops_v - ops_old << "," << *inserts_v - inserts_old << "," << *lookups_v - lookups_old << ","
                     << ((page_count)*ycsb::pageSize) / (double)1e9 << std::endl;

            ops_old = *ops_v;
            inserts_old = *inserts_v;
            lookups_old = *lookups_v;

            if (time >= FLAGS_YCSB_run_for_seconds)
               *running_v = 0;

            sleep(1);
         }
      });

      // ecall
      uint8_t wal = FLAGS_YCSB_WAL;
      if (FLAGS_YCSB_SEAL) {
         ecall_run_seal(global_eid, read_ratio, &ops, &inserts, &lookups, &running, &wal);
      } else {
         ecall_run(global_eid, read_ratio, &ops, &inserts, &lookups, &running, &wal);
      }

      stop_thread = true;
      t1.join();
   }
   destroy_enclave();
}

/* OCall functions */

void ocall_write_wal(uint8_t* data, uint32_t data_length)
{
   if (wal_buffer_idx + data_length > wal_buffer_size) {
      // reset
      wal_buffer_idx = 0;
      wal_offset += wal_buffer_size;
   }

   std::memcpy(wal_buffer_aligned + wal_buffer_idx, data, data_length);
   wal_buffer_idx += data_length;

   const int ret = pwrite(ssd_fd, wal_buffer_aligned, wal_buffer_size, wal_offset);
   if (ret != wal_buffer_size) {
      printf("Oh dear, something went wrong with write()! %s\n", strerror(errno));
      std::cout << "write failed " << ret << "\n";
   }
   fsync(ssd_fd);
}

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
   std::cout << "counter " << *number << std::endl;
}
