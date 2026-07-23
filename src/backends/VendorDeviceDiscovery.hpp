// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Areeb Sherjil
//
// Created by asherjil on 4/11/26.
//

#ifndef ABTEDGE_VENDORDEVICEDISCOVERY_HPP
#define ABTEDGE_VENDORDEVICEDISCOVERY_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>

/// Discovery policy: locate a PCI device by vendor/device ID.
/// No driver unbinding or bus-master enable — suitable for FPGAs, CPLDs, etc.
class VendorDeviceDiscovery {
public:
    // Rule of 5: move-only
    VendorDeviceDiscovery(std::uint16_t vendorID, std::uint16_t deviceID, int bar);
    VendorDeviceDiscovery(const VendorDeviceDiscovery&) = delete;
    VendorDeviceDiscovery& operator=(const VendorDeviceDiscovery&) = delete;
    VendorDeviceDiscovery(VendorDeviceDiscovery&& other) noexcept;
    VendorDeviceDiscovery& operator=(VendorDeviceDiscovery&& other) noexcept;
    ~VendorDeviceDiscovery();

    // PCIeDiscovery concept interface
    [[nodiscard]] bool        prepare() const;
    [[nodiscard]] const char* resourcePath() const;
    [[nodiscard]] std::size_t barSize() const;
    void                      release();

private:
    std::filesystem::path m_resourcePath{};
    std::size_t m_barSize{};
};

#endif //ABTEDGE_VENDORDEVICEDISCOVERY_HPP
