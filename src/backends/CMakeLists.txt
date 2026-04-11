# =============================================================================
# Backend Library Sources
# =============================================================================

target_sources(abtedge_backend PRIVATE
    BackendBase.cpp
    VendorDeviceDiscovery.cpp
    InterfaceDiscovery.cpp
    HugepageBuffer.cpp
    ../x86_64Tuner.cpp
)

target_include_directories(abtedge_backend PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

# Required for shm_open/shm_unlink (ShmBackend)
target_link_libraries(abtedge_backend PUBLIC rt)
