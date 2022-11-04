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
// -------------------------------------------------------------------------------------

DECLARE_double(GHZ);  // GHz in cycles per nanosec of CPU
DECLARE_uint64(DATA_SIZE_MiB);
DECLARE_string(csvFile);
DECLARE_string(SGX_VERSION);
DECLARE_bool(REVERSE_BATCH_ORDER);
DECLARE_uint64(MODULO_PARAM);  // used in exp to avoid compiler optimization