# Build and run:
#   docker build -t sgx-dev-container -f Dockerfile .

## generic setup for cpp and clion
FROM ubuntu:20.04
SHELL ["/bin/bash", "-c"]

ARG DEBIAN_FRONTEND=noninteractive

## SGX SDK Version
ARG DISTRO="ubuntu20.04-server"
ARG MAJOR_VERSION="2.16"
ARG BUILD="2.16.100.4"
ARG OPENSSL_VERSION="1.1.1n"
ARG SGX_MODE_FLAG="SIM"
ARG NUM_MAKE_THREADS="8"

# Install standard packages
RUN echo "Europe/Berlin" > /etc/timezone && apt-get update && apt-get upgrade -y \
&& apt-get install --no-install-recommends -y \
      build-essential gcc g++ gdb clang make cmake \
      locales-all dos2unix \
      rsync tar wget git gnupg \
      python python-dev \
      perl tzdata ca-certificates \
      nasm libssl-dev \
      && apt-get clean

# Add intel sgx repository and install sgx packages
RUN echo 'deb [arch=amd64] https://download.01.org/intel-sgx/sgx_repo/ubuntu focal main' > /etc/apt/sources.list.d/intel-sgx.list \
&& wget -qO - https://download.01.org/intel-sgx/sgx_repo/ubuntu/intel-sgx-deb.key | apt-key add - \
&& apt-get update \
&& apt-get install -y \
      libsgx-epid libsgx-quote-ex libsgx-dcap-ql \
      libsgx-urts-dbgsym libsgx-enclave-common-dbgsym libsgx-dcap-ql-dbgsym libsgx-dcap-default-qpl-dbgsym libsgx-enclave-common-dev libsgx-dcap-ql-dev libsgx-dcap-default-qpl-dev \
&& apt-get clean

# Install intel sgx sdk
WORKDIR /opt/intel
RUN wget -q https://download.01.org/intel-sgx/sgx-linux/${MAJOR_VERSION}/distro/${DISTRO}/sgx_linux_x64_sdk_${BUILD}.bin \
&& chmod +x sgx_linux_x64_sdk_${BUILD}.bin \
&& ./sgx_linux_x64_sdk_${BUILD}.bin --prefix /opt/intel \
&& rm sgx_linux_x64_sdk_${BUILD}.bin
ENV SGX_SDK=/opt/intel/sgxsdk
ENV PATH=$PATH:$SGX_SDK/bin:$SGX_SDK/bin/x64
ENV PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$SGX_SDK/pkgconfig
ENV LD_LIBRARY_PATH=$SGX_SDK/sdk_libs

WORKDIR /tmp
# Install intel-sgx-ssl
## install mitigation tools
RUN mkdir mitigation && cd mitigation \
    && wget https://download.01.org/intel-sgx/sgx-linux/${MAJOR_VERSION}/as.ld.objdump.r4.tar.gz \
    && tar -xzf as.ld.objdump.r4.tar.gz \
    && cp external/toolset/ubuntu20.04/* /usr/local/bin \
    && cd .. && rm -rf mitigation

## clone intel-sgx-ssl repo
RUN git clone --depth 1 --branch lin_${MAJOR_VERSION}_${OPENSSL_VERSION} https://github.com/intel/intel-sgx-ssl.git \
## Download OpenSSL package
    && wget -O intel-sgx-ssl/openssl_source/openssl-${OPENSSL_VERSION}.tar.gz https://www.openssl.org/source/openssl-${OPENSSL_VERSION}.tar.gz \
    && . /opt/intel/sgxsdk/environment \
    # the make command exists with a non-zero code, but compilation is usually successful
    && cd intel-sgx-ssl/Linux && make -j ${NUM_MAKE_THREADS} test SGX_MODE=${SGX_MODE_FLAG}; make install SGX_MODE=${SGX_MODE_FLAG} \
    && cd /tmp && rm -rf intel-sgx-ssl

WORKDIR /