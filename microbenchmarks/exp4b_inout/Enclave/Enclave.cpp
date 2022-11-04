#include "Enclave_t.h"  // structs defined in .edl file etc
#include "sgx_trts.h"   // trusted runtime system, usually always required

#include "../defs.h"

void ecall_main(uint64_t* data,
                uint64_t scan_length,
                uint64_t* ops,
                uint64_t* ecall_counter,
                uint64_t* modulo_param,
                uint64_t* result)
{
   (*ecall_counter)++;
   uint64_t local_result = *result;  // result to prevent optimization
   // counter to measure num ops from outside enclave
   volatile uint64_t* ops_v = ops;

   for (uint64_t i = 0; i < scan_length; i++) {
      local_result += (data[i] % *modulo_param);
      (*ops_v)++;
   }
   *result = local_result;
}