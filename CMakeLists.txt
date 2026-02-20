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
)

# Architecture-specific flags (can't use -march=native when cross-compiling)
if(CMAKE_CROSSCOMPILING)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
        # ARM64 cross-compilation: use generic ARMv8-A
        target_compile_options(fpga_driver_flags INTERFACE -march=armv8-a)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        # x86_64 cross-compilation: use generic x86-64
        target_compile_options(fpga_driver_flags INTERFACE -march=x86-64 -mtune=generic)
    endif()
else()
    # Native compilation: optimize for the build machine
    target_compile_options(fpga_driver_flags INTERFACE -march=native -mtune=native)
endif()

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

add_library(fpga_backend STATIC)

target_link_libraries(fpga_backend PUBLIC
        fpga_common
)

# Platform-specific sources added via target_sources in subdirectory
add_subdirectory(src/backends)

# =============================================================================
# Executables
# =============================================================================

# --- Latency Test (measures register write latency via oscilloscope) ---
add_executable(latency_test
        src/test/main.cpp
        src/test/WRENTester.cpp
)

target_include_directories(latency_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/test
)

target_link_libraries(latency_test PRIVATE
        fpga_backend
)

# =============================================================================
# Testing (only for native builds)
# =============================================================================

#option(BUILD_TESTING "Build unit tests" OFF)

#if(BUILD_TESTING)
#    enable_testing()
#    add_subdirectory(tests)
#endif()

# =============================================================================
# Installation
# =============================================================================

include(GNUInstallDirs)

# Install executables (uncomment when they are built)
# install(TARGETS test_harness fpga_server fpga_client_example
#         RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
# )

install(DIRECTORY src/common src/backends
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/fpga_driver
        FILES_MATCHING PATTERN "*.hpp"
)

# =============================================================================
# Summary
# =============================================================================

message(STATUS "")
message(STATUS "Targets:")
message(STATUS "  - fpga_backend        : Backend library (STATIC)")
message(STATUS "  - latency_test        : Register write latency test")
message(STATUS "")
