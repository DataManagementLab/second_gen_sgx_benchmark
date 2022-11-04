#!/bin/bash

##################################################################
# Constants and configs
##################################################################
NUM_RUNS=1

build="$HOME/second_gen_sgx_benchmark/build/microbenchmarks/exp5_ycsb"
target="ycsb"
##################################################################
# Main script starts here
##################################################################
for (( i=0; i<=$NUM_RUNS; i++ ))
do
        numactl --membind=0 -C 0 $build/$target -YCSB_records=10000000
        rm ./*.wal
        target="ycsb_untrusted"
        numactl --membind=0 -C 0 $build/$target -YCSB_records=10000000
        rm ./*.wal
done
