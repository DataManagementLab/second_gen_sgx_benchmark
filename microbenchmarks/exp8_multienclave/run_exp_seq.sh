#!/bin/bash

##################################################################
# Constants and configs
##################################################################
SGX_VERSION="sgx2"
GHZ=3.5
EPC_MAX=64000 # in MB set in the bios
NUM_NUMA=2 # number of numa nodes in system
EPC_TOTAL=$(( $EPC_MAX * $NUM_NUMA ))
toMB=$((1024 * 1024))   


config="./Enclave/Enclave.config.xml"
target="exp8_multienclave"
build="$HOME/damon22_sgx_paper/build/microbenchmarks/$target"

wait_time_sec=30

##################################################################
wait_for_files () {
##################################################################
#temp copy for iterating
check_array=("$@")
num_files=${#check_array[@]}
echo "num files is ${num_files}"
index=0
while [ "${index}" -lt "${num_files}" ]
do
    echo "num elments in array ${#check_array[@]} array ${check_array[*]}"
    file_to_check="/tmp/${check_array[$index]}"
    echo "Checking for file $file_to_check"
    if [ -f "$file_to_check" ]; then
        index=$(($index+1))
        echo "found file $file_to_check now at index $index"
        check_array=( "${check_array[@]/$file_to_check}" )
        # array=( “${array[@]/$delete}” )
        rm $file_to_check
        continue
    fi
    echo "Waiting ${wait_time_sec} sec for files to appear"
    sleep ${wait_time_sec}
done
echo "found all files"
}

##################################################################
# Main script starts here
##################################################################
dataSize_mib=64000
HeapMaxSize=$(awk -v OFMT='%.0f' "BEGIN {print ${dataSize_mib} * 1024*1024}")
pagealign=$(awk -v OFMT='%f' "BEGIN {print ${HeapMaxSize} / 4096}")
echo "Starting experiment HeapMaxSize of $HeapMaxSize Byte is pagealigned $pagealign"
sed -i "5s|.*|  <HeapMaxSize>$HeapMaxSize</HeapMaxSize>|" $config
cd $build/../..
make $target -j
cd -

NUM_ENCLAVES=$(($1-1))
# Do setup
enclave_files=() 

triggerFile="/tmp/start_experiment.txt"
touch $triggerFile
for (( i=0; i<=$NUM_ENCLAVES; i++ ))
do
    enclave_files+=("e$i.txt")
    # start process in background
    numactl --membind=0 -C $i $build/$target --ENCLAVE_ID $i --EPC_MAX_MB $EPC_MAX --NUM_NUMA_NODES $NUM_NUMA --ENC_MAXHEAP_B $HeapMaxSize > "e$i.log" 2>&1 &
    sleep 2000
done

# Trigger Experiment
wait_time_sec=600
echo "Wait for EXPERIMENT"
wait_for_files "${enclave_files[@]}"
rm $triggerFile

# Rename csv files
for f in *.csv;do
echo "$f" "sequential_${f%.*}.${f##*.}"
done

echo "Experiment done all enclaves finished"
