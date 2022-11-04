#pragma once
// -------------------------------------------------------------------------------------
#include <iostream>
#include "gflags/gflags.h"
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
// Mandatory flags (see .cpp file for validators)
DECLARE_uint64(ENC_MAXHEAP_B);
DECLARE_uint64(EPC_MAX_MB);
DECLARE_uint64(NUM_NUMA_NODES);
DECLARE_uint64(ENCLAVE_ID);
// -------------------------------------------------------------------------------------

DECLARE_double(GHZ);  // GHz in cycles per nanosec of CPU
DECLARE_string(csvFile);
DECLARE_string(SGX_VERSION);