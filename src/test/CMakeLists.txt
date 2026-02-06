# SPDX-License-Identifier: Zlib
# SPDX-FileCopyrightText: 2022 CERN (home.cern)
# SPDX-FileContributor: Author: Sylvain Fargier <sylvain.fargier@cern.ch>

cmake_minimum_required(VERSION 3.17)
cmake_policy(SET CMP0048 NEW) # project() command manages VERSION variables

set(EDGE_VERSION "4.1")
set(DRV_NAME "dummy")
set(HW_SIM_LIB "")

project("${DRV_NAME}"
  LANGUAGES C
  DESCRIPTION "${DRV_NAME} EDGE driver"
  VERSION "1.0")
message(STATUS "Building ${CMAKE_PROJECT_NAME}: ${CMAKE_PROJECT_VERSION}")

string(REGEX MATCH "^([0-9]+)\\.([0-9]+)(\\.([0-9]+))?"
       EDGE_VERSION_MATCH ${EDGE_VERSION})
set(EDGE_VERSION_MAJOR ${CMAKE_MATCH_1})
set(EDGE_VERSION_MINOR ${CMAKE_MATCH_2})
set(EDGE_VERSION_PATCH ${CMAKE_MATCH_4})

if(NOT EDGE_PATH)
  set(EDGE_PATH "/acc/local/$ENV{CPU}/drv/edge/${EDGE_VERSION}")
endif()

find_path(EDGE_INCLUDE_DIRS NAMES edge/libedge.h
  HINTS
  "${EDGE_PATH}/include"
  "/usr/local/edge/${EDGE_VERSION}/include")

find_library(EDGE_LIBRARIES NAMES edge
  HINTS
  "${EDGE_PATH}/lib"
  "/usr/local/edge/${EDGE_VERSION}/lib")

find_program(EDGE_EXECUTABLE NAMES edge edge.sh
  HINTS
  "${EDGE_PATH}/bin"
  "/acc/local/share/edge/edge-venvs/edge-${EDGE_VERSION_MAJOR}.${EDGE_VERSION_MINOR}/bin"
  "/usr/local/edge"
  "/acc/local/Linux/bin"
  "/acc/.local/share/edge")

if(NOT EDGE_INCLUDE_DIRS OR NOT EDGE_LIBRARIES OR NOT EDGE_EXECUTABLE)
  message(SEND_ERROR "Failed to find edge")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include_directories(
  "${EDGE_INCLUDE_DIRS}"
  "${CMAKE_SOURCE_DIR}")

add_custom_target(gen-sim-tpl
  COMMAND ${EDGE_EXECUTABLE} gen-sim-tpl --name "${DRV_NAME}"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${PROJECT_SOURCE_DIR}/sim"
  COMMAND ${CMAKE_COMMAND} -E rename "${DRV_NAME}" "sim"
  COMMAND ${CMAKE_COMMAND} -B "${PROJECT_BINARY_DIR}" "${PROJECT_SOURCE_DIR}")

add_library(dummy SHARED
  libdummy.c dummy_hw_desc.c)
target_link_libraries(dummy PUBLIC "${EDGE_LIBRARIES}" rt)
target_compile_definitions(dummy PUBLIC "EDGE_CPU_TAG=\"$ENV{CPU}\"")
set_target_properties(dummy PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}")

if(EXISTS ${PROJECT_SOURCE_DIR}/sim)
  file(GLOB_RECURSE SIMSRCS RELATIVE "${PROJECT_SOURCE_DIR}"
    "sim/*.c" "sim/*.cpp" "sim/*.h" "sim/*.hpp")

  add_library(dummy_sim SHARED
    libdummy_sim.c ${SIMSRCS})
  target_link_libraries(dummy_sim PUBLIC "${EDGE_LIBRARIES}" rt)
  set_target_properties(dummy_sim PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}")
endif()