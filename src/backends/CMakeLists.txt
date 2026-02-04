# =============================================================================
# Backend Library Sources
# =============================================================================

target_sources(fpga_backend PRIVATE
    BackendBase.cpp
)

target_include_directories(fpga_backend PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
