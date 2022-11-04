#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <immintrin.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <cstring>  // for memset
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>
#include "../Defs.hpp"
#include "../Helper.hpp"
#include "PerfEvent.hpp"
#include "config.h"

uint64_t highest_key = 0;
uint64_t seed = 19650218ULL;
ycsb::BTree<ycsb::Key, ycsb::Value> tree;

uint64_t wal_buffer_size = 0;
uint64_t wal_offset = 0;
uint64_t wal_buffer_idx = 0;
char* wal_buffer = nullptr;
char* wal_buffer_aligned = nullptr;
std::string path = "./ycsb_wal_untrusted.wal";
FILE* ssd_fd;

static __inline__ int64_t rdtsc(void)
{
   _mm_lfence();  // optionally wait for earlier insns to retire before reading the clock
   unsigned int lo, hi;
   __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
   _mm_lfence();  // optionally block later instructions until rdtsc retires
   return ((uint64_t)hi << 32) | lo;
}

void write_wal(uint8_t* data, uint32_t data_length)
{
   auto ret = fwrite(data, 1, data_length, ssd_fd);
   if (ret != data_length) {
      std::cout << "Wrote: " << ret << " instead of: " << data_length << std::endl;
      perror("The following error occurred");
      throw std::runtime_error("fwrite failed");
   }
   fflush(ssd_fd);

   // if (wal_buffer_idx + data_length > wal_buffer_size)
   // {
   //    // reset
   //    wal_buffer_idx = 0;
   //    wal_offset += wal_buffer_size;
   // }

   // std::memcpy(wal_buffer_aligned + wal_buffer_idx, data, data_length);
   // wal_buffer_idx += data_length;

   // const int ret = pwrite(ssd_fd, wal_buffer_aligned, data_length, wal_offset);
   // if (ret != data_length)
   // {
   //    printf("Oh dear, something went wrong with write()! %s\n", strerror(errno));
   //    std::cout << "write failed " << ret << "\n";
   // }
}

int init_wal()
{
   ssd_fd = fopen(path.c_str(), "a");

   // // make sure wal file exists
   // std::ofstream wal_file;
   // std::ofstream::openmode open_flags = std::ios::app;
   // wal_file.open(path, std::ios::trunc); // this overwrite any existing file
   // wal_file.close();
   // struct stat fstat;
   // stat(path.c_str(), &fstat);
   // int blksize = (int)fstat.st_blksize;
   // int align = blksize - 1;

   // wal_buffer_size = blksize;

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

void run(uint64_t data_size, uint64_t runs, uint64_t* cycles, uint64_t* num_measurements)
{
   // for latencies
   uint64_t counter = 0;
   uint64_t measurements = 0;
   std::atomic<uint64_t> cb = rdtsc();
   std::atomic<uint64_t> ce;

   auto* value_buffer = new uint8_t[bytes_upper_bound];

   for (uint64_t i = 1; i < runs; i++) {
      write_wal(value_buffer, data_size);  // write wal
      ce = rdtsc();
      uint64_t duration = ce.load() - cb.load();
      cycles[measurements] = duration;
      cb.store(ce.load());
      measurements++;
   }

   *num_measurements = measurements;
}

int main(int argc, char* argv[])
{
   // -------------------------------------------------------------------------------------
   gflags::SetUsageMessage("IO benchmark");
   gflags::ParseCommandLineFlags(&argc, &argv, true);
   // -------------------------------------------------------------------------------------

   init_wal();

   for (uint64_t data_size = 1; data_size < bytes_upper_bound; data_size = data_size << 1) {
      // measure query latencies
      uint64_t* cycles = new uint64_t[NUM_LATENCIES];
      uint64_t num_measurements = 0;

      // ecall
      uint8_t wal = FLAGS_YCSB_WAL;
      run(data_size, NUM_RUNS, cycles, &num_measurements);

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
      std::cout << "Current data size: " << data_size << std::endl;
      csv::write_csv(std::cout, "measurements", "min", "max", "99%", "99.9%", "average", "median");
      // result
      csv::write_csv(std::cout, measurements, min, max, p99, p999, avg, median);
      {
         // header
         std::ofstream csv_file;
         std::ofstream::openmode open_flags = std::ios::app;
         std::string file_name = "io_latencies_untrusted.csv";
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
}
