#pragma once
// -------------------------------------------------------------------------------------
#include <iostream>
#include "gflags/gflags.h"
// -------------------------------------------------------------------------------------
DECLARE_double(GHZ);  // GHz in cycles per nanosec of CPU
DECLARE_uint64(SIZE_MB);
DECLARE_uint64(PREALLOCATE);
DECLARE_bool(SHUFFLE);
DECLARE_string(csvFile);