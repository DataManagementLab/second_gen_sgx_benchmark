#include <assert.h>
#include <errno.h>
#include <fcntl.h>
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
int ssd_fd = -1;

int write_wal(ycsb::Value& value)
{
   if (wal_buffer_idx + sizeof(ycsb::Value) > wal_buffer_size) {
      // reset
      wal_buffer_idx = 0;
      wal_offset += wal_buffer_size;
   }

   std::memcpy(wal_buffer_aligned + wal_buffer_idx, reinterpret_cast<u8*>(&value), sizeof(ycsb::Value));
   wal_buffer_idx += sizeof(ycsb::Value);

   const int ret = pwrite(ssd_fd, wal_buffer_aligned, wal_buffer_size, wal_offset);
   if (ret != wal_buffer_size) {
      printf("Oh dear, something went wrong with write()! %s\n", strerror(errno));
      std::cout << "write failed " << ret << "\n";
   }
   fsync(ssd_fd);
   return 0;
}

int init_wal()
{
   // make sure wal file exists
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

void ycsb_create(uint64_t num_records, uint64_t* page_count)
{
   using namespace ycsb;
   // -------------------------------------------------------------------------------------
   Value v;
   for (Key r_i = 0; r_i < num_records; r_i++) {
      getRandString(reinterpret_cast<u8*>(&v), sizeof(Value), seed);
      tree.insert(r_i, v);
   }
   // -------------------------------------------------------------------------------------
   (*page_count) = tree.getPageCount();
   highest_key = num_records - 1;
}

void ycsb_run(uint64_t read_ratio, uint64_t* ops, uint64_t* inserts, uint64_t* lookups, uint8_t* running)
{
   using namespace ycsb;
   // counter
   volatile uint64_t* ops_v = ops;
   volatile uint64_t* inserts_v = inserts;
   volatile uint64_t* lookups_v = lookups;

   while (*running == 1) {
      // read
      Key key = rnd(seed) % highest_key;  // get key
      auto operation = rnd(seed) % 100;   // get ratio
      Value v;
      if (operation < read_ratio) {
         // do read
         auto success = tree.lookup(key, v);
         if (!success) {
            throw std::runtime_error("unexpected error");
         }
         (*lookups_v)++;
      } else {
         getRandString(reinterpret_cast<u8*>(&v), sizeof(Value), seed);
         tree.insert(key, v);
         (*inserts_v)++;
         if (FLAGS_YCSB_WAL)
            write_wal(v);  // write wal
      }
      (*ops_v)++;
   }
   if (ops == 0) {
      throw std::runtime_error("unexpected error");
   }
}

int main(int argc, char* argv[])
{
   // -------------------------------------------------------------------------------------
   gflags::SetUsageMessage("Internal hash table benchmark");
   gflags::ParseCommandLineFlags(&argc, &argv, true);
   // -------------------------------------------------------------------------------------

   ycsb::Value value;
   init_wal();

   uint64_t page_count = 0;
   {
      std::cout << "B-Tree creation ";
      ycsb_create(FLAGS_YCSB_records, &page_count);
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

         std::string file_name = "ycsb_untrusted";
         file_name = FLAGS_YCSB_WAL ? file_name += "_wal" : file_name;
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

      // ycsb
      ycsb_run(read_ratio, &ops, &inserts, &lookups, &running);
      stop_thread = true;
      t1.join();
   }
}
