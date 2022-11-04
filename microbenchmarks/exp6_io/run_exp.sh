#!/bin/bash

##################################################################
# Constants and configs
##################################################################
NUM_RUNS=1

build="$HOME/second_gen_sgx_benchmark/build/microbenchmarks/exp6_io"
target="exp6_io"
##################################################################
# Main script starts here
##################################################################
for (( i=0; i<=$NUM_RUNS; i++ ))
do
        numactl --membind=0 -C 0 $build/$target
        rm *.wal
        numactl --membind=0 -C 0 $build/$target -YCSB_SEAL
        rm *.wal
        target="exp6_io_untrusted"
        numactl --membind=0 -C 0 $build/$target
        rm *.wal
done
