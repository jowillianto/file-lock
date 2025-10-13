cmake_minimum_required(VERSION 3.28)
project(jowi_file_lock VERSION 1.0.0)
set (CMAKE_CXX_STANDARD 23)
set (CMAKE_CXX_STANDARD_REQUIRED true)
set (CMAKE_CXX_SCAN_FOR_MODULES true)

option (JOWI_FILE_LOCK_BUILD_PYTHON "Builds the python bindings" OFF)
option (JOWI_FILE_LOCK_BUILD_TESTS "Build Tests" OFF)

if (NOT DEFINED JOWI_FILE_LOCK_PYTHON_VERSION)
  set (JOWI_FILE_LOCK_PYTHON_VERSION "3.12")
endif()
set (JOWI_COMPONENT_NAME "file_lock")

if (JOWI_FILE_LOCK_BUILD_PYTHON)
  find_package(Python3 ${JOWI_FILE_LOCK_PYTHON_VERSION} EXACT COMPONENTS Development Interpreter)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/libs/pybind11)
endif()

add_library(${PROJECT_NAME})
add_library(jowi::file_lock ALIAS ${PROJECT_NAME})
if (UNIX)
  file (
    GLOB ${PROJECT_NAME}_src
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cc"
    "${CMAKE_CURRENT_LIST_DIR}/src/unix/*.cc"
  )
elseif (WIN32)
  file (
    GLOB ${PROJECT_NAME}_src
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cc"
    "${CMAKE_CURRENT_LIST_DIR}/src/win32/*.cc"
  )
endif()

target_sources(${PROJECT_NAME}
  PUBLIC
    FILE_SET CXX_MODULES FILES ${${PROJECT_NAME}_src}
)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

# PYTHON TARGET
if (JOWI_FILE_LOCK_BUILD_PYTHON)
  pybind11_add_module(${PROJECT_NAME}_python ${CMAKE_CURRENT_LIST_DIR}/python_wrapper/file_lock.cc)
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
if(JOWI_FILE_LOCK_BUILD_TESTS)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../test-lib test-lib)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../process process)
  include (CTest)
  jowi_add_test (
    ${PROJECT_NAME}_tests
    ${CMAKE_CURRENT_LIST_DIR}/tests/tests.cc
    SANITIZERS all
    LIBRARIES
      ${PROJECT_NAME}
      jowi::test_lib
      jowi::process
    COMPILE_DEFINITIONS
      TEST_CHILD="$<TARGET_FILE:test_child>"
  )
  add_executable(test_child ${CMAKE_CURRENT_LIST_DIR}/tests/test_child.cc)
  target_link_libraries(test_child ${PROJECT_NAME} jowi::test_lib)
endif()

if (JOWI_INSTALL)
  include(GNUInstallDirs)
  set (JOWI_COMPONENT_NAME "file_lock")
  set_property(
    TARGET ${PROJECT_NAME}
    PROPERTY EXPORT_NAME ${JOWI_COMPONENT_NAME}
  )
  install (
    TARGETS ${PROJECT_NAME}
    EXPORT jowi
    FILE_SET
      CXX_MODULES
      DESTINATION ${CMAKE_INSTALL_PREFIX}/cxx_modules/jowi/${JOWI_COMPONENT_NAME}
      COMPONENT ${JOWI_COMPONENT_NAME}_file_set
    CXX_MODULES_BMI
      COMPONENT ${JOWI_COMPONENT_NAME}
      DESTINATION ${CMAKE_INSTALL_PREFIX}/cxx_modules/jowi/${JOWI_COMPONENT_NAME}
  )
  if (NOT JOWI_GLOBAL_INSTALL)
    install (
      EXPORT jowi
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/jowi
      FILE moderna-config.cmake
      NAMESPACE jowi::
      CXX_MODULES_DIRECTORY cxx_modules/jowi
    )
  endif()
endif()
