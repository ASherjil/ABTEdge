# =============================================================================
# FPGA Low-Latency Driver
# =============================================================================

cmake_minimum_required(VERSION 3.21)

project(fpga_driver
        VERSION 1.0.0
        DESCRIPTION "ABTEdge"
        LANGUAGES CXX
)

# =============================================================================
# Global Settings
# =============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for clangd/CLion
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Platform Detection
# =============================================================================

if(NOT DEFINED FPGA_PLATFORM)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
        set(FPGA_PLATFORM "ARM_AXI")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        set(FPGA_PLATFORM "X86_PCIE")
    else()
        set(FPGA_PLATFORM "SIMULATED")
    endif()
endif()

message(STATUS "======================================")
message(STATUS "FPGA Driver Configuration")
message(STATUS "======================================")
message(STATUS "Platform:      ${FPGA_PLATFORM}")
message(STATUS "Processor:     ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Build type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler:  ${CMAKE_CXX_COMPILER}")
message(STATUS "======================================")

# =============================================================================
# Compiler Flags
# =============================================================================

add_library(fpga_driver_flags INTERFACE)

target_compile_options(fpga_driver_flags INTERFACE
        # Warnings
        -Wall
        -Wextra
        -Wpedantic
        -Werror=return-type
        -Wconversion
        -Wshadow

        # Performance (always on for low-latency)
        -march=native
        -mtune=native
)

# Release-specific flags
target_compile_options(fpga_driver_flags INTERFACE
        $<$<CONFIG:Release>:
        -O3
        -DNDEBUG
        -flto
        -fno-exceptions      # Optional: disable for lowest latency
        >
)

# Debug-specific flags
target_compile_options(fpga_driver_flags INTERFACE
        $<$<CONFIG:Debug>:
        -O0
        -g3
        -fno-omit-frame-pointer
        -fsanitize=address,undefined
        >
)

target_link_options(fpga_driver_flags INTERFACE
        $<$<CONFIG:Debug>:
        -fsanitize=address,undefined
        >
        $<$<CONFIG:Release>:
        -flto
        >
)

# =============================================================================
# Common Library (header-only: concepts, SPSC queue, messages)
# =============================================================================

add_library(fpga_common INTERFACE)

target_include_directories(fpga_common INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(fpga_common INTERFACE
        fpga_driver_flags
)

target_compile_definitions(fpga_common INTERFACE
        FPGA_PLATFORM_${FPGA_PLATFORM}
)

# =============================================================================
# Backend Library (platform-specific hardware access)
# =============================================================================

add_library(fpga_backend INTERFACE)

target_link_libraries(fpga_backend INTERFACE
        fpga_common
)

# Platform-specific sources added via target_sources in subdirectory
add_subdirectory(src/backends)

# =============================================================================
# Executables
# =============================================================================

# --- Test Harness (Phase 1: isolated hardware testing) ---
add_executable(test_harness
        src/test_harness/main.cpp
)

target_link_libraries(test_harness PRIVATE
        fpga_backend
        rt          # POSIX shared memory
        pthread
)

# --- FPGA Server (Phase 3: production with IPC) ---
add_executable(fpga_server
        src/server/main.cpp
)

target_link_libraries(fpga_server PRIVATE
        fpga_backend
        rt
        pthread
)

# --- Example Client ---
add_executable(fpga_client_example
        src/client/example_client.cpp
)

target_link_libraries(fpga_client_example PRIVATE
        fpga_common
        rt
        pthread
)

# =============================================================================
# Testing (only for native builds)
# =============================================================================

option(BUILD_TESTING "Build unit tests" OFF)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# =============================================================================
# Installation
# =============================================================================

include(GNUInstallDirs)

install(TARGETS test_harness fpga_server fpga_client_example
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY src/common src/backends src/client
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fpga_driver
        FILES_MATCHING PATTERN "*.hpp"
)

# =============================================================================
# Summary
# =============================================================================

message(STATUS "")
message(STATUS "Targets:")
message(STATUS "  - test_harness        : Isolated hardware testing")
message(STATUS "  - fpga_server         : Production server with IPC")
message(STATUS "  - fpga_client_example : Example client application")
if(BUILD_TESTING)
    message(STATUS "  - tests               : Unit tests enabled")
endif()
message(STATUS "")
