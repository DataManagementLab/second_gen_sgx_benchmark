#include "config.h"

// -------------------------------------------------------------------------------------
// Mandatory flags
static bool isSet(const char* flagname, uint64_t value)
{
   if (value != 0)  // value is ok
   {
      return true;
   }
   std::cout << "Invalid value for --" << flagname << ": " << value << ". Please set value" << std::endl;
   return false;
}
DEFINE_uint64(ENC_MAXHEAP_B, 0, "MaxHeapSize (bytes) that is configured in the Enclave.config.xml");
DEFINE_validator(ENC_MAXHEAP_B, &isSet);
DEFINE_uint64(EPC_MAX_MB, 0, "size in mb");
DEFINE_validator(EPC_MAX_MB, &isSet);
DEFINE_uint64(NUM_NUMA_NODES, 0, "number of sockets/numa nodes");
DEFINE_validator(NUM_NUMA_NODES, &isSet);
// -------------------------------------------------------------------------------------
DEFINE_double(GHZ, 3.5, "GHz in cycles per nanosec of CPU");
DEFINE_uint64(DATA_SIZE_MiB, 32768, "Size of the data to process inside enclave in MB");
DEFINE_string(csvFile, "result.csv", "Path to csv file in which result will be stored");
DEFINE_string(SGX_VERSION, "sgx2", "sgx1 or sgx2");
DEFINE_bool(REVERSE_BATCH_ORDER, false, "Run experiment beginning with largest batch size first ");
DEFINE_uint64(MODULO_PARAM, 100, "Will be used to compute modulo");  // used in exp to avoid compiler optimization
