#include "config.h"

DEFINE_bool(YCSB_WAL, false, "wal");
DEFINE_bool(YCSB_SEAL, false, "seal wal data");
DEFINE_uint64(YCSB_records, 1000, "number records");
DEFINE_double(YCSB_run_for_seconds, 30.0, "run seconds");
DEFINE_double(GHZ, 3.5, "GHz in cycles per nanosec of CPU");
