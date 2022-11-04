add_library(shared_headers INTERFACE)
target_include_directories(shared_headers INTERFACE ./include ${SGX_INCLUDE_DIR})