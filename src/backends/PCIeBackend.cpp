//
// Created by asherjil on 2/6/26.
//

#include "backends/PCIeBackend.hpp"

#include <string>
#include <fstream>     // std::ifstream
#include <cstdio>      // sscanf, std::printf
#include <sys/mman.h>  // mmap, MAP_HUGETLB, etc.
#include <fcntl.h>     // ::open, O_RDWR, O_SYNC
#include <unistd.h>    // ::close

namespace fs = std::filesystem;

PCIeBackend::PCIeBackend(std::uint16_t vendorID, std::uint16_t deviceID, int bar){
    auto readHex = [](const fs::path& p) -> std::uint16_t {
        std::ifstream f(p);
        std::uint16_t value{};
        f >> std::hex >> value;
        return value;
    };

    for (const auto& entry : fs::directory_iterator(PCIE_DIRECTORY)) {
        // Each device has "/sys/bus/pci/devices/xxx/vendor" or "/sys/bus/pci/devices/xxx/device"
        if (readHex(entry.path() / "vendor") != vendorID) continue;
        if (readHex(entry.path() / "device") != deviceID) continue;

        // We found the correct device matching the vendor/device ID
        m_pcieResourcePath = entry.path() / ("resource" + std::to_string(bar));

        // Now we need to find the size from the resource text file
        // The file has 6 lines (BAR0..BAR5), each formatted as:
        //   "0x00000000f7000000 0x00000000f7ffffff 0x0000000000040200"
        //    ^start              ^end                ^flags
        // Size = end - start + 1
        std::ifstream resFile(entry.path() / "resource");
        std::string line{};
        for (int i{}; i<=bar && std::getline(resFile, line); i++) {}

        std::uint64_t start{}, end{};
        sscanf(line.c_str(), "%lx %lx", &start, &end);
        m_barSize = end - start + 1;

        std::fprintf(stderr, "DEBUG: path=%s start=0x%lX end=0x%lX size=%zu\n",
             m_pcieResourcePath.c_str(), start, end, m_barSize);

        break; // stop here we found our match
    }

    if (m_pcieResourcePath.empty()) {
        std::fprintf(stderr, "PCIeBackend: no device found with vendor=0x%04X device=0x%04X\n",
                     vendorID, deviceID);
    }
}

PCIeBackend::PCIeBackend(PCIeBackend&& other) noexcept
    : BackendBase(std::move(other)),
      m_pcieResourcePath(std::move(other.m_pcieResourcePath)),
      m_barSize(other.m_barSize) {
    other.m_barSize = 0;
}
PCIeBackend& PCIeBackend::operator=(PCIeBackend&& other) noexcept {
    if (this != &other) {
        BackendBase::operator=(std::move(other));
        m_pcieResourcePath = std::move(other.m_pcieResourcePath);
        m_barSize = other.m_barSize;
        other.m_barSize = 0;
    }
    return *this;
}

bool PCIeBackend::open(){
    return BackendBase::open(m_pcieResourcePath.c_str(), 0, m_barSize);
}

