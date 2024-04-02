cmake_minimum_required(VERSION 3.28)
project(multiprocessing_fileLock)
set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED true)
set (CMAKE_CXX_SCAN_FOR_MODULES true)
add_compile_options("-Ofast")

add_library(multiprocessing_fileLock)
file (
  GLOB file_lock_ccm
  "${CMAKE_CURRENT_LIST_DIR}/src/*.ccm"
)
target_sources(multiprocessing_fileLock
  PUBLIC
    FILE_SET CXX_MODULES FILES ${file_lock_ccm}
)
add_subdirectory(lib/test-lib EXCLUDE_FROM_ALL)
add_executable(
  multiprocessing_file_lock_tests
  EXCLUDE_FROM_ALL 
  ${CMAKE_CURRENT_LIST_DIR}/tests.cpp
)
target_link_libraries(multiprocessing_file_lock_tests 
  test_lib 
  multiprocessing_fileLock
)