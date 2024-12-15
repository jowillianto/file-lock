cmake_minimum_required(VERSION 3.28)
project(moderna_file_lock)
set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED true)
set (CMAKE_CXX_SCAN_FOR_MODULES true)

find_package(Python3 3.7 REQUIRED COMPONENTS Interpreter Development)

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
if (EXISTS ${CMAKE_CURRENT_LIST_DIR}/lib/pybind11/CMakeLists.txt)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/pybind11 EXCLUDE_FROM_ALL)
  add_library(${PROJECT_NAME}_python SHARED EXCLUDE_FROM_ALL)
  target_sources(${PROJECT_NAME}_python
    PUBLIC
      ${CMAKE_CURRENT_LIST_DIR}/python_wrapper/file_lock.cpp
    PUBLIC FILE_SET CXX_MODULES FILES ${${PROJECT_NAME}_src}
  )
  target_include_directories(${PROJECT_NAME}_python 
    PUBLIC 
      ${pybind11_INCLUDE_DIRS}
      ${Python_INCLUDE_DIRS}
  )
  target_link_libraries(${PROJECT_NAME}_python 
      ${pybind11_LIBRARIES}
      pybind11::module
  )
  set_target_properties(${PROJECT_NAME}_python PROPERTIES PREFIX "")
  set_target_properties(${PROJECT_NAME}_python PROPERTIES OUTPUT_NAME file_lock)
  set_target_properties(${PROJECT_NAME}_python PROPERTIES SUFFIX ".so")
  target_compile_options(${PROJECT_NAME}_python PRIVATE "-fPIC")
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