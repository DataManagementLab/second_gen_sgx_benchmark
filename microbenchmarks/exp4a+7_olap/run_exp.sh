#!/bin/bash

# heap size in MB
HEAP_SIZE=( 1 2 4 8 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144)

build="$HOME/second_gen_sgx_benchmark/build/microbenchmarks/exp4a+7_olap"

factor_byte=$((1024 * 1024))   
fration_data_size=0.8

#trusted execution
echo "executing trusted code"
#token=1
#start=$token

config="./Enclave/Enclave.config.xml"


for i in "${HEAP_SIZE[@]}"
do
    byte=$((${i} * ${factor_byte}))
    echo ${byte}
    ds=$(bc <<< "scale=0; (${byte} * ${fration_data_size})/${factor_byte}")
    echo ${ds}
     
    #sed -i "5s/${start}/${byte}/" ${config}
    sed -i "5s|.*|  <HeapMaxSize>$byte</HeapMaxSize>|" $config
    #start=${byte}
    cat ${config}

    cd ${build}
    pwd
    make -j
    echo "numactl --membind=0 -C 0 ./exp4a_olap -HEAP_SIZE=${byte} -DS_MB=${ds}"
    numactl --membind=0 -C 0 ./exp4a_olap -HEAP_SIZE=${byte} -DS_MB=${ds}
    cd -
done
# restore initial state
#sed -i "5s/${start}/${token}/" ${config}


# untrusted execution

echo "executing untrusted code "

for i in "${HEAP_SIZE[@]}"
do
    byte=$((${i} * ${factor_byte}))
    echo ${byte}
    ds=$(bc <<< "scale=0; (${byte} * ${fration_data_size})/${factor_byte}")
    echo ${ds}

    cd ${build}
    pwd
    make -j 
    echo "numactl --membind=0 -C 0 ./exp4a_olap_untrusted -HEAP_SIZE=${byte} -DS_MB=${ds}"
    numactl --membind=0 -C 0 ./exp4a_olap_untrusted -HEAP_SIZE=${byte} -DS_MB=${ds}
    cd -
done
