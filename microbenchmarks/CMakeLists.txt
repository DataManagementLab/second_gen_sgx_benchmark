list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
# FetchContent added in CMake 3.11, downloads during the configure step
include(FetchContent)

# declare dependencies
find_package(SGX REQUIRED) # sgx sdk

# gflags for parsing commandline flags
FetchContent_Declare(
  gflags
  GIT_REPOSITORY https://github.com/gflags/gflags.git
  GIT_TAG v2.2.2)
FetchContent_MakeAvailable(gflags)

# include subprojects / targets
add_subdirectory(perfevent-master)
add_subdirectory(SharedHeaders)
add_subdirectory(exp1_btree)
add_subdirectory(exp2_numa)
add_subdirectory(exp3_paging)
add_subdirectory(exp4a+7_olap)
add_subdirectory(exp4b_inout)
add_subdirectory(exp5_ycsb)
add_subdirectory(exp6_io)
add_subdirectory(exp8_multienclave)
