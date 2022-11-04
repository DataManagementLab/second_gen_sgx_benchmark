#!/bin/bash

##################################################################
# Constants and configs
##################################################################
NUM_RUNS=1

build="$HOME/second_gen_sgx_benchmark/build/microbenchmarks/exp1_btree"
target="exp1_btree"
##################################################################
# Main script starts here
##################################################################
for (( i=0; i<=$NUM_RUNS; i++ ))
do
        numactl --membind=0 -C 0 $build/$target 
    
done
