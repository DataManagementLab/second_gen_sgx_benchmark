#pragma once
// -------------------------------------------------------------------------------------
#include <iostream>
#include "gflags/gflags.h"
// -------------------------------------------------------------------------------------
DECLARE_bool(YCSB_WAL);
DECLARE_bool(YCSB_SEAL);
DECLARE_uint64(YCSB_records);
DECLARE_double(YCSB_run_for_seconds);
