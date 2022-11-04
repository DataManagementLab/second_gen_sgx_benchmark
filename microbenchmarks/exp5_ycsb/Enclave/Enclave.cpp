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

ycsb::BTree<ycsb::Key, ycsb::Value> tree;
std::string path = "./ycsb_wal_protected.wal";
SGX_FILE* wal_fh = nullptr;

void ecall_create(uint64_t num_records, uint64_t* page_count, uint8_t wal)
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

   if (wal) {
      std::string mode = "a";
      wal_fh = sgx_fopen_auto_key(path.c_str(), mode.c_str());
   }
}

size_t write_wal(ycsb::Value& value)
{
   size_t ret = sgx_fwrite(reinterpret_cast<u8*>(&value), sizeof(value), 1, wal_fh);
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

void ecall_run(uint64_t read_ratio, uint64_t* ops, uint64_t* inserts, uint64_t* lookups, uint8_t* running, uint8_t* wal)
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
         if (wal)
            write_wal(v);  // write wal
      }
      (*ops_v)++;
   }
   if (ops == 0) {
      throw std::runtime_error("unexpected error");
   }
}

void ecall_run_seal(uint64_t read_ratio,
                    uint64_t* ops,
                    uint64_t* inserts,
                    uint64_t* lookups,
                    uint8_t* running,
                    uint8_t* wal)
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
         uint8_t* sealed_buffer = nullptr;
         // uintt seal_data_size = seal_data(reinterpret_cast<u8 *>(&v), sizeof(Value), (sgx_sealed_data_t *)
         // sealed_buffer); // write wal
         uint64_t data_length = sizeof(v);
         uint32_t sealed_size = sgx_calc_sealed_data_size((uint32_t)strlen(aad_mac_text), data_length);
         if (sealed_size != 0) {
            sealed_buffer = (uint8_t*)malloc(sealed_size);
            sgx_status_t err = sgx_seal_data((uint32_t)strlen(aad_mac_text), (const uint8_t*)aad_mac_text, data_length,
                                             reinterpret_cast<u8*>(&v), sealed_size, (sgx_sealed_data_t*)sealed_buffer);
            if (sealed_buffer == nullptr) {
               throw std::runtime_error("unexpected error");
            }
            if (wal)
               ocall_write_wal(sealed_buffer, sealed_size);  // write wal
         }
         (*ops_v)++;
      }
      if (ops == 0) {
         throw std::runtime_error("unexpected error");
      }
   }
}
