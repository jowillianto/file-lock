cmake_minimum_required(VERSION 3.28)
project(multiprocessing_fileLock)
set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED true)
set (CMAKE_CXX_SCAN_FOR_MODULES true)

find_package(Python3 3.7 REQUIRED COMPONENTS Interpreter Development)

add_library(multiprocessing_fileLock)
file (
  GLOB file_lock_ccm
  "${CMAKE_CURRENT_LIST_DIR}/src/*.ccm"
)

target_sources(multiprocessing_fileLock
  PUBLIC
    FILE_SET CXX_MODULES FILES ${file_lock_ccm}
)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/pybind11 EXCLUDE_FROM_ALL)
add_library(
  multiprocessing_file_lock_python SHARED EXCLUDE_FROM_ALL
  ${CMAKE_CURRENT_LIST_DIR}/python_wrapper/file_lock.cpp
)
target_include_directories(multiprocessing_file_lock_python 
  PUBLIC 
    ${pybind11_INCLUDE_DIRS}
    ${Python_INCLUDE_DIRS}
)
target_link_libraries(multiprocessing_file_lock_python 
    ${pybind11_LIBRARIES}
    multiprocessing_fileLock 
    pybind11::module
)
set_target_properties(multiprocessing_file_lock_python PROPERTIES PREFIX "")
set_target_properties(multiprocessing_file_lock_python PROPERTIES OUTPUT_NAME file_lock)
set_target_properties(multiprocessing_file_lock_python PROPERTIES SUFFIX ".so")

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