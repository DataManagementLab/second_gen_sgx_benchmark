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
DEFINE_uint64(MULTIPLE_EPC_TOTAL, 1, "data size as multiple of EPC_TOTAL, e.g. 2 => 2*EPC_MAX*2");
DEFINE_string(csvFile, "result.csv", "Path to csv file in which result will be stored");
