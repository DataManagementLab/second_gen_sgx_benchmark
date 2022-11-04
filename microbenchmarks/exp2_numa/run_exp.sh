#!/bin/bash

##################################################################
# Constants and configs
##################################################################
SGX_VERSION="sgx2"
GHZ=3.5
NUM_RUNS=1
DATA_SIZE=2000
PREALLOCATION=70000

build="$HOME/second_gen_sgx_benchmark/build/microbenchmarks/exp2_numa"
resultFile="$(hostname)_numa_latencies.csv"
##################################################################
# Main script starts here
##################################################################
for (( i=0; i<=$NUM_RUNS; i++ ))
do
    for shuffle in true false
    do
        target="NUMAExp_untrusted"
        # untrusted local numa
        numactl --membind=0 -C 0 $build/$target --PREALLOCATE 0 --SIZE_MB $DATA_SIZE --SHUFFLE=$shuffle --csvFile $resultFile
        # untrusted cross numa
        numactl --membind=1 -C 0 $build/$target --PREALLOCATE $PREALLOCATION --SIZE_MB $DATA_SIZE --SHUFFLE=$shuffle --csvFile $resultFile

        target="NUMAExp"
        # trusted local numa
        numactl --membind=0 -C 0 $build/$target --PREALLOCATE 0 --SIZE_MB $DATA_SIZE --SHUFFLE=$shuffle --csvFile $resultFile
        # trusted cross numa
        numactl --membind=0 -C 0 $build/$target --PREALLOCATE $PREALLOCATION --SIZE_MB $DATA_SIZE --SHUFFLE=$shuffle --csvFile $resultFile
    done
    
done
