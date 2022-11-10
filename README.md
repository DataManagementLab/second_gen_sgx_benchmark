# Benchmarking the Second Generation of Intel SGX Hardware
This is the source code for our (Muhammad El-Hindi, Tobias Ziegler, Matthias Heinrich, Adrian Lutsch, Zheguang Zhao, Carsten Binnig) published paper at DAMON'22: Benchmarking the Second Generation of Intel SGX Hardware

The paper can be found here:  [Paper Link](hhttps://dl.acm.org/doi/10.1145/3533737.3535098)

## Abstract
In recent years, trusted execution environments (TEEs) such as Intel Software Guard Extensions (SGX) have gained a lot of attention in the database community. This is because TEEs provide an interesting platform for building trusted databases in the cloud. However, until recently SGX was only available on low-end single socket servers built on the Intel Xeon E3 processor generation and came with many restrictions for building DBMSs. With the availability of the new Ice Lake processors, Intel provides a new implementation of the SGX technology that supports high-end multi-socket servers. With this new implementation, which we refer to as SGXv2 in this paper, Intel promises to address several limitations of SGX enclaves. This raises the question whether previous efforts to overcome the limitations of SGX for DBMSs are still applicable and if the new generation of SGX can truly deliver on the promise to secure data without compromising on performance. To answer this question, in this paper we conduct a first systematic performance study of Intel SGXv2 and compare it to the previous generation of SGX.

## Citation
```bib
@inproceedings{10.1145/3533737.3535098,
author = {El-Hindi, Muhammad and Ziegler, Tobias and Heinrich, Matthias and Lutsch, Adrian and Zhao, Zheguang and Binnig, Carsten},
title = {Benchmarking the Second Generation of Intel SGX Hardware},
year = {2022},
isbn = {9781450393782},
publisher = {Association for Computing Machinery},
address = {New York, NY, USA},
url = {https://doi.org/10.1145/3533737.3535098},
doi = {10.1145/3533737.3535098},
booktitle = {Data Management on New Hardware},
articleno = {5},
numpages = {8},
location = {Philadelphia, PA, USA},
series = {DaMoN'22}
}
```

## Requirements
To run the code in this repo make sure to install the following dependencies:
- CMake, gcc, clang and other build utils
- Intel SGX SDK
- Intel SGX SSL

Have a lock at `docker/Dockerfile` as reference how to setup your system for compilation.

## Building the code
Configure and build on SGX HW (remove the `SGX_HW` flag to use simulation mode)
```
cmake -S . -B build -DSGX_HW=ON -DSGX_MODE="PreRelease"
cmake --build build --parallel
```

## Docker
We provide a docker image for a faster dev environment setup.

Build the image:
```
cd docker
docker build -t sgx-dev-container -f Dockerfile .
```

Interactive use:
```
docker run -it -u $(id -u ${USER}):$(id -g ${USER}) -v $(pwd):$(pwd) -w $(pwd) sgx-dev-container bash 
```

Only build using container:
```
docker run -u $(id -u ${USER}):$(id -g ${USER}) -v $(pwd):$(pwd) -w $(pwd) sgx-dev-container bash -c "cmake -S . -B build && cmake --build build --parallel"
```

## Re-producing experiment results
1) Compile the code in HW+PreRelease mode as mentioned above
2) Run the `run_exp.sh` scripts in the `/microbenchmarks` folder
