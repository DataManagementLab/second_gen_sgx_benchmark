#include "config.h"

DEFINE_double(GHZ, 3.5, "GHz in cycles per nanosec of CPU");
DEFINE_uint64(SIZE_MB, 1, "size in mb");
DEFINE_uint64(PREALLOCATE, 70e3, "size in mb");
DEFINE_bool(SHUFFLE, true, "Do random scan");
DEFINE_string(csvFile, "result.csv", "Path to csv file in which result will be stored");
