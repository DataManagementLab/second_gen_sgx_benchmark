#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <thread>
#include "../Defs.hpp"
#include "../Helper.hpp"
#include "Enclave_u.h"
#include "PerfEvent.hpp"
#include "config.h"
#include "sgxerrors.h"

#define ENCLAVE_FILENAME "exp6_ioenclave.signed.so"  // linux

sgx_enclave_id_t global_eid = 0;

uint64_t wal_buffer_size = 0;
uint64_t wal_offset = 0;
uint64_t wal_buffer_idx = 0;
char* wal_buffer = nullptr;
char* wal_buffer_aligned = nullptr;
std::string path = "./ycsb_wal_trusted.wal";
FILE* ssd_fd;

int init_wal()
{
   ssd_fd = fopen(path.c_str(), "a");

   // path = FLAGS_YCSB_SEAL ? path +="_seal" : path ;
   // // make sure that wal file exists and is empty
   // std::ofstream wal_file;
   // std::ofstream::openmode open_flags = std::ios::app;
   // wal_file.open(path, std::ios::trunc); // this overwrite any existing file
   // wal_file.close();
   // struct stat fstat;
   // stat(path.c_str(), &fstat);
   // int blksize = (int)fstat.st_blksize;
   // int align = blksize - 1;

   // wal_buffer_size = blksize;

   // // int flags = O_RDWR | O_DIRECT;
   // int flags = O_RDWR;
   // ssd_fd = open(path.c_str(), flags, 0666);
   // if (ssd_fd == -1)
   // {
   //    printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
   // }
   // wal_buffer = new char[((int)blksize + align)];
   // wal_buffer_aligned = (char *)(((uintptr_t)wal_buffer + align) & ~((uintptr_t)align));

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
   gflags::SetUsageMessage("Internal hash table benchmark");
   gflags::ParseCommandLineFlags(&argc, &argv, true);
   // -------------------------------------------------------------------------------------

   std::cout << "Sealing " << (FLAGS_YCSB_SEAL ? "true" : "false") << "\n";

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
      std::cout << "WAL creation ";
      ecall_create(global_eid);
      std::cout << "  [OK ]" << std::endl;
   }

   for (uint64_t data_size = 1; data_size < bytes_upper_bound; data_size = data_size << 1) {
      // measure query latencies
      uint64_t* cycles = new uint64_t[NUM_LATENCIES];
      uint64_t num_measurements = 0;

      // ecall
      uint8_t wal = FLAGS_YCSB_WAL;
      if (FLAGS_YCSB_SEAL) {
         ecall_run_seal(global_eid, data_size, NUM_RUNS, cycles, &num_measurements);
      } else {
         ecall_run(global_eid, data_size, NUM_RUNS, cycles, &num_measurements);
      }

      // -------------------------------------------------------------------------------------
      // print latencies
      std::sort(cycles, cycles + num_measurements);

      std::vector<double> cycles_vec(num_measurements);
      std::transform(cycles, cycles + num_measurements, cycles_vec.begin(),
                     [](uint64_t cycles) -> double { return (cycles); });

      // median
      uint64_t measurements = num_measurements;
      double min = cycles_vec[0];
      double max = cycles_vec.back();
      double p99 = cycles_vec[num_measurements * 0.99];
      double p999 = cycles_vec[num_measurements * 0.999];
      double avg = std::accumulate(cycles_vec.begin(), cycles_vec.end(), 0.0) / cycles_vec.size();
      double median = cycles_vec[num_measurements * 0.5];

      // header
      std::cout << "Current data size" << data_size << std::endl;
      csv::write_csv(std::cout, "measurements", "min", "max", "99%", "99.9%", "average", "median");
      // result
      csv::write_csv(std::cout, measurements, min, max, p99, p999, avg, median);
      {
         // header
         std::ofstream csv_file;
         std::ofstream::openmode open_flags = std::ios::app;
         std::string file_name = "io_latencies_trusted.csv";
         bool csv_initialized = std::filesystem::exists(file_name);
         csv_file.open(file_name, open_flags);
         // print header
         if (!csv_initialized) {
            csv_file << "latency_cycles, datasize, seal" << std::endl;
         }

         for (auto& cyc : cycles_vec) {
            csv::write_csv(csv_file, cyc, data_size, (FLAGS_YCSB_SEAL ? "true" : "false"));
         }
      }
   }
   destroy_enclave();
}

/* OCall functions */

void ocall_write_wal(uint8_t* data, uint32_t data_length)
{
   auto ret = fwrite(data, 1, data_length, ssd_fd);
   if (ret != data_length)
      throw std::runtime_error("fwrite failed");
   fflush(ssd_fd);
   //    if (wal_buffer_idx + data_length > wal_buffer_size)
   //    {
   //       // reset
   //       wal_buffer_idx = 0;
   //       wal_offset += wal_buffer_size;
   //    }

   //    std::memcpy(wal_buffer_aligned + wal_buffer_idx, data, data_length);
   //    wal_buffer_idx += data_length;

   //    const int ret = pwrite(ssd_fd, wal_buffer_aligned, data_length, wal_offset);
   //    if (ret != data_length)
   //    {
   //       printf("Oh dear, something went wrong with write()! %s\n", strerror(errno));
   //       std::cout << "write failed " << ret << "\n";
   //    }
   // fsync(ssd_fd);
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
