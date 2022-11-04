#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <cstring>  // for memset
#include <filesystem>
#include <fstream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>
#include "PerfEvent.hpp"
#include "config.h"

uint64_t rnd(uint64_t& seed)
{
   uint64_t x = seed; /* state nicht mit 0 initialisieren */
   x ^= x >> 12;      // a
   x ^= x << 25;      // b
   x ^= x >> 27;      // c
   seed = x;
   return x * 0x2545F4914F6CDD1D;
}

int main(int argc, char* argv[])
{
   // -------------------------------------------------------------------------------------
   gflags::SetUsageMessage("Internal hash table benchmark");
   gflags::ParseCommandLineFlags(&argc, &argv, true);
   // -------------------------------------------------------------------------------------
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
         const std::string file_name = host + "olap_measurements_untrusted.csv";
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

      {
         using Value = uint64_t;
         Value result = 0;  // result to prevent optimization
         uint64_t seed = 19650218ULL;

         std::vector<Value> table(number_values);
         std::iota(std::begin(table), std::end(table), 0);

         initialized = 1;  // kind of barrier
         // counter
         volatile uint64_t* ops_v = &ops;
         volatile uint8_t* running_v = &running;

         uint64_t index = 0;
         while (*running_v == 1) {
            // scan seq or random
            if (sequential == 1) {
               result += table[index++ % (number_values)];
            } else {
               index = rnd(seed) % (number_values);  // get key
               result += table[index];
            }
            (*ops_v)++;
         }

         std::cout << result << std::endl;
         std::cout << ops << std::endl;
      }

      stop_thread = true;
      t1.join();
      std::cout << "counter test " << ops << std::endl;
   }
}
