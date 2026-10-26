# =============================================================================
# ARM64 Cross-Compilation Toolchain (Zynq UltraScale+ / AXI)
# =============================================================================

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# CERN CDK toolchain paths
# CDK Debian release: 12 (deb12x64/deb12a64) or 13 (deb13x64/deb13a64).
# Set by the deb13* presets; defaults to 12.
if(NOT DEFINED CDK_DEBIAN_VERSION)
    set(CDK_DEBIAN_VERSION 12)
endif()
# Forward it to CMake's try_compile projects (compiler checks), or they fall back to 12.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CDK_DEBIAN_VERSION)
set(TOOLCHAIN_ROOT "/acc/sys/cdk/debian/${CDK_DEBIAN_VERSION}/aarch64/sysroots/host/usr/bin")

set(CMAKE_C_COMPILER   "${TOOLCHAIN_ROOT}/aarch64-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_ROOT}/aarch64-linux-gnu-g++")

# LTO requires gcc-ar and gcc-ranlib (they have the LTO plugin)
set(CMAKE_AR           "${TOOLCHAIN_ROOT}/aarch64-linux-gnu-gcc-ar")
set(CMAKE_RANLIB       "${TOOLCHAIN_ROOT}/aarch64-linux-gnu-gcc-ranlib")

# gcc-ar/gcc-ranlib exec a plain `ar`/`ranlib`. Without -B they can pick the host's
# /bin/ar, which cannot load this toolchain's LTO plugin (glibc mismatch), and the
# archive ends up without an LTO symbol index (undefined references at link time).
set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> -B${TOOLCHAIN_ROOT} qc <TARGET> <LINK_FLAGS> <OBJECTS>")
set(CMAKE_CXX_ARCHIVE_APPEND "<CMAKE_AR> -B${TOOLCHAIN_ROOT} q <TARGET> <LINK_FLAGS> <OBJECTS>")
set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -B${TOOLCHAIN_ROOT} <TARGET>")

# Search paths for cross-compilation
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Platform identifier for our code
set(ABTEDGE_ARCH "AARCH64" CACHE STRING "Target platform" FORCE)