cmake_minimum_required(VERSION 3.28)
project(multiprocessing_fileLock)
set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED true)
set (CMAKE_CXX_SCAN_FOR_MODULES true)

find_package(Python3 3.7 REQUIRED COMPONENTS Interpreter Development)
find_package(Boost REQUIRED COMPONENTS Python)

add_library(multiprocessing_fileLock)
file (
  GLOB file_lock_ccm
  "${CMAKE_CURRENT_LIST_DIR}/src/*.ccm"
)
target_sources(multiprocessing_fileLock
  PUBLIC
    FILE_SET CXX_MODULES FILES ${file_lock_ccm}
)
add_library(
  multiprocessing_fileLock_python SHARED EXCLUDE_FROM_ALL
  ${CMAKE_CURRENT_LIST_DIR}/python_wrapper/file_lock.cpp
)
target_include_directories(multiprocessing_fileLock_python 
  PUBLIC 
    ${Boost_INCLUDE_DIR}
    ${Python3_INCLUDE_DIRS}
)
# target_link_libraries(multiprocessing_fileLock_python 
#     ${Boost_LIBRARIES}
#     Python3
#     multiprocessing_fileLock 
# )

add_subdirectory(lib/test-lib EXCLUDE_FROM_ALL)
add_executable(
  multiprocessing_file_lock_tests
  EXCLUDE_FROM_ALL 
  ${CMAKE_CURRENT_LIST_DIR}/tests/tests.cpp
)
target_link_libraries(multiprocessing_file_lock_tests 
  test_lib 
  multiprocessing_fileLock
)
target_compile_options(multiprocessing_file_lock_tests PUBLIC "-O0")