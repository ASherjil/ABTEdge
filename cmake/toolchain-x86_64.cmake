# =============================================================================
# x86_64 Toolchain (PCIe FPGA - TXMC635)
# =============================================================================

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# CERN CDK toolchain paths
set(TOOLCHAIN_ROOT "/acc/sys/cdk/debian/12/x86_64/sysroots/host/usr/bin")

set(CMAKE_C_COMPILER   "${TOOLCHAIN_ROOT}/gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_ROOT}/g++")

# Platform identifier for our code
set(FPGA_PLATFORM "X86_PCIE" CACHE STRING "Target platform" FORCE)