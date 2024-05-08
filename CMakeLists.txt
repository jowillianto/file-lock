cmake_minimum_required(VERSION 3.28)
project(multiprocessing_file_lock)
set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED true)
set (CMAKE_CXX_SCAN_FOR_MODULES true)

find_package(Python3 3.7 REQUIRED COMPONENTS Interpreter Development)

add_library(multiprocessing_file_lock)
file (
  GLOB file_lock_ccm
  "${CMAKE_CURRENT_LIST_DIR}/src/*.ccm"
)

target_sources(multiprocessing_file_lock
  PUBLIC
    FILE_SET CXX_MODULES FILES ${file_lock_ccm}
)

# PYTHON TARGET
if (EXISTS ${CMAKE_CURRENT_LIST_DIR}/lib/pybind11/CMakeLists.txt)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/pybind11 EXCLUDE_FROM_ALL)
  add_library(multiprocessing_file_lock_python SHARED EXCLUDE_FROM_ALL)
  target_sources(multiprocessing_file_lock_python
    PUBLIC
      ${CMAKE_CURRENT_LIST_DIR}/python_wrapper/file_lock.cpp
    PUBLIC FILE_SET CXX_MODULES FILES ${file_lock_ccm}
  )
  target_include_directories(multiprocessing_file_lock_python 
    PUBLIC 
      ${pybind11_INCLUDE_DIRS}
      ${Python_INCLUDE_DIRS}
  )
  target_link_libraries(multiprocessing_file_lock_python 
      ${pybind11_LIBRARIES}
      pybind11::module
  )
  set_target_properties(multiprocessing_file_lock_python PROPERTIES PREFIX "")
  set_target_properties(multiprocessing_file_lock_python PROPERTIES OUTPUT_NAME file_lock)
  set_target_properties(multiprocessing_file_lock_python PROPERTIES SUFFIX ".so")
  target_compile_options(multiprocessing_file_lock_python PRIVATE "-fPIC")
endif()

# TEST TARGET
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/lib/test-lib/CMakeLists.txt)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/test-lib EXCLUDE_FROM_ALL)
  add_executable(multiprocessing_file_lock_test
    EXCLUDE_FROM_ALL 
    ${CMAKE_CURRENT_LIST_DIR}/tests/tests.cpp
  )
  target_link_libraries(multiprocessing_file_lock_test
    PRIVATE
      test_lib 
      multiprocessing_file_lock
  )
  add_executable(multiprocessing_file_lock_test_leak_check
    EXCLUDE_FROM_ALL 
    ${CMAKE_CURRENT_LIST_DIR}/tests/tests.cpp
  )
  target_link_libraries(multiprocessing_file_lock_test_leak_check
    PRIVATE
      test_lib 
      multiprocessing_file_lock
  )
  target_compile_options(multiprocessing_file_lock_test_leak_check PRIVATE "-fsanitize=address")
  target_link_options(multiprocessing_file_lock_test_leak_check PRIVATE "-fsanitize=address")
endif()