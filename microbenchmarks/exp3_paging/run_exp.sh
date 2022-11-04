#!/bin/bash

EPC_MAX=8000 # in MB set in the bios
NUM_NUMA=2 # number of numa nodes in system
EPC_TOTAL=$(( $EPC_MAX * $NUM_NUMA ))
toMB=$((1024 * 1024))   


config="./Enclave/Enclave.config.xml"
target="exp3_paging"
build="$HOME/second_gen_sgx_benchmark/build/microbenchmarks/$target"
resultFile="$(hostname)_${EPC_MAX}_pagingOverhead.csv"
# Allocate only on one numa node => maxHeap = EPC_MAX & total_mb < maxHeap
# HeapMaxSize in byte!
for multipleEPC in 1 2 4 8 16 
do
HeapMaxSize=$(awk -v OFMT='%.0f' "BEGIN {print ${multipleEPC} * ${EPC_TOTAL} * 1000000}")
pagealign=$(awk -v OFMT='%f' "BEGIN {print ${HeapMaxSize} / 4096}")
echo "Starting experiment for multiple epc of $multipleEPC with HeapMaxSize of $HeapMaxSize Byte is pagealigned $pagealign"
ts=$(date +%s)
sed -i "5s|.*|  <HeapMaxSize>$HeapMaxSize</HeapMaxSize>|" $config
cd $build
make -j
echo "numactl --membind=0 -C 0 $build/$target --MULTIPLE_OF_EPC_TOTAL $multipleEPC --EPC_MAX_MB $EPC_MAX --NUM_NUMA_NODES $NUM_NUMA --ENC_MAXHEAP_B $HeapMaxSize --csvFile $resultFile"
numactl --membind=0 -C 0 $build/$target --MULTIPLE_EPC_TOTAL $multipleEPC --EPC_MAX_MB $EPC_MAX --NUM_NUMA_NODES $NUM_NUMA --ENC_MAXHEAP_B $HeapMaxSize --csvFile $resultFile
cd -

done

