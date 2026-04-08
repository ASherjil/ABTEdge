# =============================================================================
# ABTEdge — Ultra-low latency kernel-bypass hardware access library
# =============================================================================

cmake_minimum_required(VERSION 3.21)

project(abtedge
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

if(NOT DEFINED ABTEDGE_ARCH)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
        set(ABTEDGE_ARCH "AARCH64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        set(ABTEDGE_ARCH "X86_64")
    else()
        set(ABTEDGE_ARCH "SIMULATED")
    endif()
endif()

message(STATUS "======================================")
message(STATUS "ABTEdge Configuration")
message(STATUS "======================================")
message(STATUS "Platform:      ${ABTEDGE_ARCH}")
message(STATUS "Processor:     ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Build type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler:  ${CMAKE_CXX_COMPILER}")
message(STATUS "======================================")

# =============================================================================
# Compiler Flags
# =============================================================================

add_library(abtedge_flags INTERFACE)

target_compile_options(abtedge_flags INTERFACE
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
        target_compile_options(abtedge_flags INTERFACE -march=armv8-a)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        # x86_64 cross-compilation: use generic x86-64
        target_compile_options(abtedge_flags INTERFACE -march=x86-64 -mtune=generic)
    endif()
else()
    # Native compilation: optimize for the build machine
    target_compile_options(abtedge_flags INTERFACE -march=native -mtune=native)
endif()

# Release-specific flags
# Optimisation flags are INTERFACE (propagate to consumers).
# -fno-exceptions/-fno-rtti are PRIVATE to ABTEdge targets only — consumers
# may need exceptions/RTTI, so these must not propagate.
target_compile_options(abtedge_flags INTERFACE
        $<$<CONFIG:Release>:
        -O3
        -DNDEBUG
        -flto
        -ffunction-sections
        -fdata-sections
        >
)

# Debug-specific flags
target_compile_options(abtedge_flags INTERFACE
        $<$<CONFIG:Debug>:
        -O0
        -g3
        -fno-omit-frame-pointer
        -fsanitize=address,undefined
        >
)

target_link_options(abtedge_flags INTERFACE
        $<$<CONFIG:Debug>:
        -fsanitize=address,undefined
        >
        $<$<CONFIG:Release>:
        -flto
        -Wl,--gc-sections
        >
)

# =============================================================================
# Common Library (header-only: concepts, shared memory)
# =============================================================================

add_library(abtedge_common INTERFACE)

target_include_directories(abtedge_common INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(abtedge_common INTERFACE
        abtedge_flags
)

target_compile_definitions(abtedge_common INTERFACE
        ABTEDGE_ARCH_${ABTEDGE_ARCH}
)

# =============================================================================
# Backend Library (hardware access: PCIe, AXI, shared memory)
# =============================================================================

add_library(abtedge_backend STATIC)

target_link_libraries(abtedge_backend PUBLIC
        abtedge_common
)

# Flags that are too aggressive for consumers — keep them PRIVATE to ABTEdge
target_compile_options(abtedge_backend PRIVATE
        $<$<CONFIG:Release>:
        -fno-exceptions
        -fno-rtti
        -fno-unwind-tables
        -fno-asynchronous-unwind-tables
        >
)

# Platform-specific sources added via target_sources in subdirectory
add_subdirectory(src/backends)

# =============================================================================
# Executables (only when building ABTEdge standalone, not as a dependency)
# =============================================================================

if(PROJECT_IS_TOP_LEVEL)
    # --- Latency Test (measures register write latency via oscilloscope) ---
    add_executable(latency_test
            src/test/main.cpp
            src/test/WRENTester.cpp
    )

    target_include_directories(latency_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/test
    )

    target_link_libraries(latency_test PRIVATE
            abtedge_backend
    )
endif()

# =============================================================================
# Testing (only for native builds)
# =============================================================================

#option(BUILD_TESTING "Build unit tests" OFF)

#if(BUILD_TESTING)
#    enable_testing()
#    add_subdirectory(tests)
#endif()

if(PROJECT_IS_TOP_LEVEL)
    # =============================================================================
    # Installation
    # =============================================================================

    include(GNUInstallDirs)

    install(DIRECTORY src/common src/backends
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/abtedge
            FILES_MATCHING PATTERN "*.hpp"
    )

    # =============================================================================
    # Summary
    # =============================================================================

    message(STATUS "")
    message(STATUS "Targets:")
    message(STATUS "  - abtedge_backend     : Backend library (STATIC)")
    message(STATUS "  - latency_test        : Register write latency test")
    message(STATUS "")
endif()
