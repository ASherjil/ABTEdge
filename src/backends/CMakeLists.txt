# =============================================================================
# Backend Library Sources
# =============================================================================

target_sources(fpga_backend PRIVATE
    BackendBase.cpp
    PCIeBackend.cpp
    ../x86_64Tuner.cpp
)

target_include_directories(fpga_backend PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

# Required for shm_open/shm_unlink (ShmBackend)
target_link_libraries(fpga_backend PUBLIC rt)
