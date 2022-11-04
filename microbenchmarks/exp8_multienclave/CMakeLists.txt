set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

set(EDL_SEARCH_PATHS Enclave)
set(E_SRCS Enclave/Enclave.cpp)
#makes no sense in this context, but can be useful in a real world example so that different configs can be used for debug/release
if(SGX_HW AND SGX_MODE STREQUAL "Release")
    set(LDS Enclave/Enclave.lds)
else()
    set(LDS Enclave/Enclave.lds)
endif()
add_enclave_library(exp8_multienclaveenclave SRCS ${E_SRCS} EDL Enclave/Enclave.edl EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS} LDSCRIPT ${LDS})
enclave_sign(exp8_multienclaveenclave KEY Enclave/Enclave_private.pem CONFIG Enclave/Enclave.config.xml)

set(SRCS App/App.cpp App/config.cpp)
add_untrusted_executable(exp8_multienclave SRCS ${SRCS} EDL Enclave/Enclave.edl EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS})
target_include_directories(exp8_multienclave PRIVATE ${SGX_INCLUDE_DIR})
target_link_libraries(exp8_multienclave shared_headers gflags)
add_dependencies(exp8_multienclave exp8_multienclaveenclave-sign)
