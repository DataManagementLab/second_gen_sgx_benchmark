set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -g -O3")
set(EDL_SEARCH_PATHS Enclave)
set(E_SRCS Enclave/Enclave.cpp)
#makes no sense in this context, but can be useful in a real world example so that different configs can be used for debug/release
if(SGX_HW AND SGX_MODE STREQUAL "Release")
    set(LDS Enclave/Enclave.lds)
else()
    set(LDS Enclave/Enclave.lds)
endif()
add_enclave_library(exp1_btreeenclave SRCS ${E_SRCS} EDL Enclave/Enclave.edl EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS} LDSCRIPT ${LDS})
enclave_sign(exp1_btreeenclave KEY Enclave/Enclave_private.pem CONFIG Enclave/Enclave.config.xml)

option(VTUNE "Enable VTUNE" 0)
set(SRCS App/App.cpp)
add_untrusted_executable(exp1_btree SRCS ${SRCS} EDL Enclave/Enclave.edl EDL_SEARCH_PATHS ${EDL_SEARCH_PATHS})
target_compile_definitions(exp1_btree PRIVATE VTUNE=${VTUNE})
target_include_directories(exp1_btree PRIVATE ${SGX_INCLUDE_DIR})
target_link_libraries(exp1_btree shared_headers )
add_dependencies(exp1_btree exp1_btreeenclave-sign)
