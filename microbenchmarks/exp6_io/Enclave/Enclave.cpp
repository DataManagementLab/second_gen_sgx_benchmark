#include <assert.h>
#include <stdlib.h>
#include <algorithm>
#include <atomic>
#include <cstring>  // for memset
#include <numeric>
#include <string>
#include <vector>
#include "../Defs.hpp"
#include "Enclave_t.h"          // structs defined in .edl file etc
#include "sgx_tprotected_fs.h"  // sgx library for protected io
#include "sgx_trts.h"           // trusted runtime system, usually always required
#include "sgx_tseal.h"

#include <atomic>
#include <cassert>
#include <cstring>

uint64_t highest_key = 0;
uint64_t seed = 19650218ULL;
char aad_mac_text[256] = "SgxYCSBEvaluation";

std::string path = "./ycsb_wal_protected.wal";
SGX_FILE* wal_fh = nullptr;

static __inline__ int64_t rdtsc(void)
{
   __builtin_ia32_lfence();
   unsigned int lo, hi;
   __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
   __builtin_ia32_lfence();
   return ((uint64_t)hi << 32) | lo;
}

void ecall_create()
{
   using namespace ycsb;
   std::string mode = "a";
   wal_fh = sgx_fopen_auto_key(path.c_str(), mode.c_str());
}

size_t sgx_write_wal(uint8_t* value, uint64_t size)
{
   size_t ret = sgx_fwrite(value, 1, size, wal_fh);

   if (ret != size)
      throw std::runtime_error("write failed");
   // -------------------------------------------------------------------------------------
   // flush
   auto success = sgx_fflush(wal_fh);
   if (success != 0)
      throw std::runtime_error("flush failed");
   return ret;
}

uint32_t seal_data(uint8_t* data, uint64_t data_length, sgx_sealed_data_t* sealed_data)
{
   uint32_t sealed_size = sgx_calc_sealed_data_size((uint32_t)strlen(aad_mac_text), data_length);
   if (sealed_size != 0) {
      sealed_data = (sgx_sealed_data_t*)malloc(sealed_size);
      sgx_status_t err = sgx_seal_data((uint32_t)strlen(aad_mac_text), (const uint8_t*)aad_mac_text, data_length,
                                       (uint8_t*)data, sealed_size, sealed_data);
   }
   return sealed_size;
}

void ecall_run(uint64_t data_size, uint64_t runs, uint64_t* cycles, uint64_t* num_measurements)
{
   // for latencies
   uint64_t counter = 0;
   uint64_t measurements = 0;
   std::atomic<uint64_t> cb = rdtsc();
   std::atomic<uint64_t> ce;

   auto* value_buffer = new uint8_t[bytes_upper_bound];

   for (uint64_t i = 1; i < runs; i++) {
      // for(uint64_t i = 1; i< bytes_upper_bound; i= i<<2){
      // todo time
      sgx_write_wal(value_buffer, data_size);  // write wal
      ce = rdtsc();
      uint64_t duration = ce.load() - cb.load();
      cycles[measurements] = duration;
      cb.store(ce.load());
      measurements++;
   }

   *num_measurements = measurements;
}

void ecall_run_seal(uint64_t data_size, uint64_t runs, uint64_t* cycles, uint64_t* num_measurements)
{
   // for latencies
   uint64_t counter = 0;
   uint64_t measurements = 0;
   std::atomic<uint64_t> cb = rdtsc();
   std::atomic<uint64_t> ce;

   auto* value_buffer = new uint8_t[data_size];
   uint32_t max_sealed_size = sgx_calc_sealed_data_size((uint32_t)strlen(aad_mac_text), bytes_upper_bound);

   auto* sealed_buffer = new uint8_t[max_sealed_size * 2];

   for (uint64_t i = 1; i < runs; i++) {
      uint64_t data_length = data_size;
      uint32_t sealed_size = sgx_calc_sealed_data_size((uint32_t)strlen(aad_mac_text), data_size);

      sgx_status_t err = sgx_seal_data((uint32_t)strlen(aad_mac_text), (const uint8_t*)aad_mac_text, data_size,
                                       value_buffer, sealed_size, (sgx_sealed_data_t*)sealed_buffer);

      if (err != SGX_SUCCESS) {
         throw std::runtime_error("unexpected error");
      }

      if (sealed_buffer == nullptr) {
         throw std::runtime_error("unexpected error");
      }
      ocall_write_wal(sealed_buffer, sealed_size);  // write wal
      ce = rdtsc();
      uint64_t duration = ce.load() - cb.load();
      cycles[measurements] = duration;
      cb.store(ce.load());
      measurements++;
   }

   *num_measurements = measurements;
}
