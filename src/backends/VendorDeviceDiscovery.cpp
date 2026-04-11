//
// Created by asherjil on 4/11/26.
//

#include "VendorDeviceDiscovery.hpp"

#include <cinttypes>
#include <cstdio>
#include <fstream>
#include <string>
#include <utility>

namespace fs = std::filesystem;

static constexpr std::string_view PCIE_DIRECTORY = "/sys/bus/pci/devices";

VendorDeviceDiscovery::VendorDeviceDiscovery(std::uint16_t vendorID, std::uint16_t deviceID, int bar) {
    auto readHex = [](const fs::path& p) -> std::uint16_t {
        std::ifstream f(p);
        std::uint16_t value{};
        f >> std::hex >> value;
        return value;
    };

    for (const auto& entry : fs::directory_iterator(PCIE_DIRECTORY)) {
        if (readHex(entry.path() / "vendor") != vendorID) continue;
        if (readHex(entry.path() / "device") != deviceID) continue;

        m_resourcePath = entry.path() / ("resource" + std::to_string(bar));

        // Read BAR size from resource text file
        // Each line: "0xstart 0xend 0xflags", one per BAR
        std::ifstream resFile(entry.path() / "resource");
        std::string line{};
        for (int i{}; i <= bar && std::getline(resFile, line); i++) {}

        std::uint64_t start{}, end{};
        std::sscanf(line.c_str(), "%" SCNx64 " %" SCNx64, &start, &end);
        m_barSize = end - start + 1;

        std::fprintf(stderr, "[PCIe] VendorDevice: path=%s size=%zu\n",
                     m_resourcePath.c_str(), m_barSize);
        break;
    }

    if (m_resourcePath.empty()) {
        std::fprintf(stderr, "[PCIe] No device found: vendor=0x%04X device=0x%04X\n",
                     vendorID, deviceID);
    }
}

VendorDeviceDiscovery::VendorDeviceDiscovery(VendorDeviceDiscovery&& other) noexcept
    : m_resourcePath(std::move(other.m_resourcePath)),
      m_barSize(std::exchange(other.m_barSize, 0)) {}

VendorDeviceDiscovery& VendorDeviceDiscovery::operator=(VendorDeviceDiscovery&& other) noexcept {
    if (this != &other) {
        m_resourcePath = std::move(other.m_resourcePath);
        m_barSize = std::exchange(other.m_barSize, 0);
    }
    return *this;
}

VendorDeviceDiscovery::~VendorDeviceDiscovery() = default;

bool VendorDeviceDiscovery::prepare() const {
    return !m_resourcePath.empty();
}

const char* VendorDeviceDiscovery::resourcePath() const {
    return m_resourcePath.c_str();
}

std::size_t VendorDeviceDiscovery::barSize() const {
    return m_barSize;
}

void VendorDeviceDiscovery::release(){

}
