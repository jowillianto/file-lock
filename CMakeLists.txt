cmake_minimum_required(VERSION 3.28)
project(moderna_file_lock)
set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED true)
set (CMAKE_CXX_SCAN_FOR_MODULES true)

option (MODERNA_FILE_LOCK_BUILD_PYTHON "Builds the python bindings" OFF)
option (MODERNA_FILE_LOCK_PYTHON_VERSION "Python Version to use" "")

if (MODERNA_FILE_LOCK_BUILD_PYTHON)
  find_package(Python3 ${MODERNA_FILE_LOCK_PYTHON_VERSION} EXACT COMPONENTS Development)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libs/pybind11)
endif()

add_library(${PROJECT_NAME})
if (UNIX)
  file (
    GLOB ${PROJECT_NAME}_src
    "${CMAKE_CURRENT_LIST_DIR}/src/*.ccm"
    "${CMAKE_CURRENT_LIST_DIR}/src/unix/*.ccm"
  )
elseif (WIN32)
  file (
    GLOB ${PROJECT_NAME}_src
    "${CMAKE_CURRENT_LIST_DIR}/src/*.ccm"
    "${CMAKE_CURRENT_LIST_DIR}/src/win32/*.ccm"
  )
endif()

target_sources(${PROJECT_NAME}
  PUBLIC
    FILE_SET CXX_MODULES FILES ${${PROJECT_NAME}_src}
)

# PYTHON TARGET
if (MODERNA_FILE_LOCK_BUILD_PYTHON)
  pybind11_add_module(${PROJECT_NAME}_python ${CMAKE_CURRENT_LIST_DIR}/python_wrapper/file_lock.cpp)
  target_link_libraries(${PROJECT_NAME}_python 
    PUBLIC
      ${PROJECT_NAME}
      pybind11::module
  )
  set_target_properties(${PROJECT_NAME}_python PROPERTIES PREFIX "")
  set_target_properties(${PROJECT_NAME}_python PROPERTIES OUTPUT_NAME file_lock)
  set_target_properties(${PROJECT_NAME}_python PROPERTIES SUFFIX ".so")
endif()

# TEST TARGET
if(
  EXISTS ${CMAKE_CURRENT_LIST_DIR}/../test-lib/CMakeLists.txt 
  AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/../thread-plus/CMakeLists.txt
  AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/../process/CMakeLists.txt
  AND PROJECT_IS_TOP_LEVEL
)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../test-lib test-lib)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../thread-plus thread-plus)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../process process)
  include (CTest)
  moderna_add_test ( 
    ${PROJECT_NAME}_tests
    ${CMAKE_CURRENT_LIST_DIR}/tests/tests.cpp
    SANITIZERS all
    LIBRARIES 
      ${PROJECT_NAME}
      moderna_test_lib 
      moderna_thread_plus 
      moderna_process
    COMPILE_DEFINITIONS 
      TEST_CHILD="$<TARGET_FILE:test_child>"
  )
  add_executable(test_child ${CMAKE_CURRENT_LIST_DIR}/tests/test_child.cpp)
  target_link_libraries(test_child ${PROJECT_NAME} moderna_test_lib)
elseif (PROJECT_IS_TOP_LEVEL)
  message("Test with moderna full library")
endif()