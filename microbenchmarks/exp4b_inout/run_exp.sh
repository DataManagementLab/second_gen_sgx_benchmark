#!/bin/bash

SGX_VERSION="sgx2"
GHZ=3.5
EPC_MAX=64000 # in MB set in the bios
NUM_NUMA=2 # number of numa nodes in system
EPC_TOTAL=$(( $EPC_MAX * $NUM_NUMA ))
toMB=$((1024 * 1024))   


config="./Enclave/Enclave.config.xml"
target="exp4b_inout"
build="$HOME/second_gen_sgx_benchmark/build/microbenchmarks/$target"

# Allocate only on one numa node => maxHeap = EPC_MAX & total_mb < maxHeap
# HeapMaxSize in byte!
for dataSize_mib in 28672
do
HeapMaxSize=$(awk -v OFMT='%.0f' "BEGIN {print ${dataSize_mib} * 1024*1024}")
pagealign=$(awk -v OFMT='%f' "BEGIN {print ${HeapMaxSize} / 4096}")
dataSize_mib=$(($dataSize_mib - 1024))
echo "Starting experiment for processing ${dataSize_mib} MiB data with HeapMaxSize of $HeapMaxSize Byte is pagealigned $pagealign"
ts=$(date +%s)
sed -i "5s|.*|  <HeapMaxSize>$HeapMaxSize</HeapMaxSize>|" $config
cd $build
make -j
echo " Run experiment one time in order and with reversed order of batches"
echo "numactl --membind=0 -C 0 $build/$target --DATA_SIZE_MiB $dataSize_mib --EPC_MAX_MB $EPC_MAX --NUM_NUMA_NODES $NUM_NUMA --ENC_MAXHEAP_B $HeapMaxSize --SGX_VERSION=${SGX_VERSION} --GHZ=$GHZ"
numactl --membind=0 -C 0 $build/$target --DATA_SIZE_MiB $dataSize_mib --EPC_MAX_MB $EPC_MAX --NUM_NUMA_NODES $NUM_NUMA --ENC_MAXHEAP_B $HeapMaxSize --SGX_VERSION=${SGX_VERSION} --GHZ=$GHZ
echo "numactl --membind=0 -C 0 $build/$target --DATA_SIZE_MiB $dataSize_mib --EPC_MAX_MB $EPC_MAX --NUM_NUMA_NODES $NUM_NUMA --ENC_MAXHEAP_B $HeapMaxSize --REVERSE_BATCH_ORDER --SGX_VERSION=${SGX_VERSION} --GHZ=$GHZ"
numactl --membind=0 -C 0 $build/$target --DATA_SIZE_MiB $dataSize_mib --EPC_MAX_MB $EPC_MAX --NUM_NUMA_NODES $NUM_NUMA --ENC_MAXHEAP_B $HeapMaxSize --REVERSE_BATCH_ORDER --SGX_VERSION=${SGX_VERSION} --GHZ=$GHZ
cd -

done

