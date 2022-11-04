#include <assert.h>
#include <stdlib.h>
#include <algorithm>
#include <atomic>
#include <cstring>  // for memset
#include <numeric>
#include <string>
#include <vector>
#include "Enclave_t.h"  // structs defined in .edl file etc
#include "sgx_trts.h"   // trusted runtime system, usually always required

#include <atomic>
#include <cassert>
#include <cstring>

uint64_t rnd(uint64_t& seed)
{
   uint64_t x = seed; /* state nicht mit 0 initialisieren */
   x ^= x >> 12;      // a
   x ^= x << 25;      // b
   x ^= x >> 27;      // c
   seed = x;
   return x * 0x2545F4914F6CDD1D;
}

void ecall_run(uint64_t* ops, uint8_t* running, uint64_t* number_values, uint8_t* sequential, uint8_t* initialized)
{
   using Value = uint64_t;
   Value result = 0;  // result to prevent optimization
   uint64_t seed = 19650218ULL;

   std::vector<Value> table(*number_values);
   std::iota(std::begin(table), std::end(table), 0);

   *initialized = 1;  // kind of barrier
   // counter
   volatile uint64_t* ops_v = ops;
   volatile uint8_t* running_v = running;

   uint64_t index = 0;
   while (*running == 1) {
      // scan seq or random
      if (*sequential == 1) {
         result += table[index++ % (*number_values)];
      } else {
         index = rnd(seed) % (*number_values);  // get key
         result += table[index];
      }
      (*ops_v)++;
   }

   ocall_print_number(&result);
   ocall_print_number(ops);
}
