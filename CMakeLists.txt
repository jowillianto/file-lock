cmake_minimum_required(VERSION 3.28)
project(moderna_file_lock)
set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED true)
set (CMAKE_CXX_SCAN_FOR_MODULES true)

find_package(Python3 3.7 REQUIRED COMPONENTS Interpreter Development)

add_library(moderna_file_lock)
file (
  GLOB moderna_file_lock_src
  "${CMAKE_CURRENT_LIST_DIR}/src/*.ccm"
)

target_sources(moderna_file_lock
  PUBLIC
    FILE_SET CXX_MODULES FILES ${moderna_file_lock_src}
)

# PYTHON TARGET
if (EXISTS ${CMAKE_CURRENT_LIST_DIR}/lib/pybind11/CMakeLists.txt)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/pybind11 EXCLUDE_FROM_ALL)
  add_library(moderna_file_lock_python SHARED EXCLUDE_FROM_ALL)
  target_sources(moderna_file_lock_python
    PUBLIC
      ${CMAKE_CURRENT_LIST_DIR}/python_wrapper/file_lock.cpp
    PUBLIC FILE_SET CXX_MODULES FILES ${file_lock_ccm}
  )
  target_include_directories(moderna_file_lock_python 
    PUBLIC 
      ${pybind11_INCLUDE_DIRS}
      ${Python_INCLUDE_DIRS}
  )
  target_link_libraries(moderna_file_lock_python 
      ${pybind11_LIBRARIES}
      pybind11::module
  )
  set_target_properties(moderna_file_lock_python PROPERTIES PREFIX "")
  set_target_properties(moderna_file_lock_python PROPERTIES OUTPUT_NAME file_lock)
  set_target_properties(moderna_file_lock_python PROPERTIES SUFFIX ".so")
  target_compile_options(moderna_file_lock_python PRIVATE "-fPIC")
endif()

# TEST TARGET
if(
  EXISTS ${CMAKE_CURRENT_LIST_DIR}/../test-lib/CMakeLists.txt 
  AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/../../cmake/CMakeLists.txt 
  AND PROJECT_IS_TOP_LEVEL
)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../test-lib test-lib)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../thread_plus thread_plus)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../process process)
  include (CTest)
  include (${CMAKE_CURRENT_LIST_DIR}/../../cmake/CMakeLists.txt)
  add_test_asan_tsan_usan_ubsan ( 
    moderna_file_lock_tests
    ${CMAKE_CURRENT_LIST_DIR}/tests/tests.cpp
    "moderna_file_lock;moderna_test_lib;moderna_thread_plus;moderna_process"
    "TEST_CHILD=\"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_child\""
  )
  add_executable(test_child ${CMAKE_CURRENT_LIST_DIR}/tests/test_child.cpp)
  target_link_libraries(test_child moderna_file_lock)
else()
  message("Test with moderna full library")
endif()