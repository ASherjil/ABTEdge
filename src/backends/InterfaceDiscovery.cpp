//
// Created by asherjil on 4/11/26.
//

#include "InterfaceDiscovery.hpp"

#include <cerrno>
#include <charconv>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <utility>

namespace fs = std::filesystem;

InterfaceDiscovery::InterfaceDiscovery(std::string_view ifname, int bar,
                                       std::string_view driverName) {
    std::error_code ec;

    // 1. Resolve BDF — try the live interface symlink first
    //    /sys/class/net/eno2/device -> ../../0000:0c:00.0
    std::string sysPath = "/sys/class/net/" + std::string(ifname) + "/device";
    auto devLink = fs::read_symlink(sysPath, ec);
    if (!ec) {
        m_bdf = devLink.filename().string();
    } else {
        // Interface not present (driver unbound from a previous run?)
        // Fall back to scanning PCI devices by BIOS index for enoN naming
        if (!resolveByBiosIndex(ifname)) {
            std::fprintf(stderr, "[PCIe] Interface %.*s does not exist and"
                                 " could not be resolved via BIOS index\n",
                         static_cast<int>(ifname.size()), ifname.data());
            return;
        }
    }

    // 2. Resolve driver name — sysfs is the source of truth, user hint is fallback
    auto drvLink = fs::read_symlink("/sys/bus/pci/devices/" + m_bdf + "/driver", ec);
    if (!ec) {
        std::string resolved = drvLink.filename().string();
        if (!driverName.empty() && resolved != driverName) {
            std::fprintf(stderr, "[PCIe] Warning: provided driver '%.*s' does not match "
                                 "bound driver '%s' — using '%s'\n",
                         static_cast<int>(driverName.size()), driverName.data(),
                         resolved.c_str(), resolved.c_str());
        }
        m_driverName = std::move(resolved);
    } else if (!driverName.empty()) {
        // Driver not bound (unbound from previous run) — use caller-provided name
        m_driverName = std::string(driverName);
    }

    // 3. Read BAR size from resource file
    if (!readBarSize(bar)) {
        return;
    }

    std::fprintf(stderr, "[PCIe] Interface %.*s -> BDF %s, driver %s, BAR%d size %zu\n",
                 static_cast<int>(ifname.size()), ifname.data(), m_bdf.c_str(),
                 m_driverName.empty() ? "(none)" : m_driverName.c_str(),
                 bar, m_barSize);
}

InterfaceDiscovery::InterfaceDiscovery(InterfaceDiscovery&& other) noexcept
    : m_bdf(std::move(other.m_bdf)),
      m_driverName(std::move(other.m_driverName)),
      m_resourcePath(std::move(other.m_resourcePath)),
      m_barSize(std::exchange(other.m_barSize, 0)) {}

InterfaceDiscovery& InterfaceDiscovery::operator=(InterfaceDiscovery&& other) noexcept {
    if (this != &other) {
        m_bdf = std::move(other.m_bdf);
        m_driverName = std::move(other.m_driverName);
        m_resourcePath = std::move(other.m_resourcePath);
        m_barSize = std::exchange(other.m_barSize, 0);
    }
    return *this;
}

InterfaceDiscovery::~InterfaceDiscovery() = default;

bool InterfaceDiscovery::prepare() {
    if (m_bdf.empty()) return false;

    if (!m_driverName.empty()) {
        if (!unbindDriver()) return false;
    }

    return enableBusMaster();
}

const char* InterfaceDiscovery::resourcePath() const {
    return m_resourcePath.c_str();
}

std::size_t InterfaceDiscovery::barSize() const {
    return m_barSize;
}

void InterfaceDiscovery::release() {
    if (m_bdf.empty()) {
        std::fprintf(stderr, "[PCIe] Warning: release() called with no BDF — device was never resolved\n");
        return;
    }

    if (!m_driverName.empty()) {
        std::fprintf(stderr, "[PCIe] Driver '%s' was unbound from %s.\n"
                             "[PCIe] To rebind: echo '%s' > /sys/bus/pci/drivers/%s/bind\n",
                     m_driverName.c_str(), m_bdf.c_str(),
                     m_bdf.c_str(), m_driverName.c_str());
    } else {
        std::fprintf(stderr, "[PCIe] Device %s has no known driver.\n"
                             "[PCIe] Pass the driver name to InterfaceDiscovery for rebind instructions.\n",
                     m_bdf.c_str());
    }
}

std::string_view InterfaceDiscovery::bdf() const {
    return m_bdf;
}

std::string_view InterfaceDiscovery::driverName() const {
    return m_driverName;
}

// When the interface is gone (driver unbound from a previous run), the enoN
// predictable name encodes the BIOS device index N.  Every PCI device exposes
// that index in /sys/bus/pci/devices/<bdf>/index, even without a driver.
// We scan for a matching Ethernet controller (PCI class 0x0200xx).
bool InterfaceDiscovery::resolveByBiosIndex(std::string_view ifname) {
    // Only enoN naming carries a BIOS index
    if (ifname.size() < 4 || ifname.substr(0, 3) != "eno") {
        std::fprintf(stderr, "[PCIe] Interface %.*s is not enoN naming — cannot resolve by BIOS index\n",
                     static_cast<int>(ifname.size()), ifname.data());
        return false;
    }

    unsigned targetIndex{};
    auto [ptr, errc] = std::from_chars(ifname.data() + 3,
                                       ifname.data() + ifname.size(),
                                       targetIndex);
    if (errc != std::errc{} || ptr != ifname.data() + ifname.size()) {
        std::fprintf(stderr, "[PCIe] Cannot parse BIOS index from '%.*s'\n",
                     static_cast<int>(ifname.size()), ifname.data());
        return false;
    }

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator("/sys/bus/pci/devices", ec)) {
        // Check PCI class: 0x020000 = Ethernet controller
        std::ifstream classFile(entry.path() / "class");
        if (!classFile.is_open()) continue;
        std::uint32_t pciClass{};
        classFile >> std::hex >> pciClass;
        if ((pciClass >> 8) != 0x0200) continue;

        // Check BIOS device index
        std::ifstream indexFile(entry.path() / "index");
        if (!indexFile.is_open()) continue;
        unsigned devIndex{};
        indexFile >> devIndex;
        if (devIndex != targetIndex) continue;

        m_bdf = entry.path().filename().string();
        std::fprintf(stderr, "[PCIe] Resolved %.*s via BIOS index %u -> BDF %s\n",
                     static_cast<int>(ifname.size()), ifname.data(),
                     targetIndex, m_bdf.c_str());
        return true;
    }

    std::fprintf(stderr, "[PCIe] No Ethernet device found with BIOS index %u\n", targetIndex);
    return false;
}

bool InterfaceDiscovery::readBarSize(int bar) {
    std::string basePath = "/sys/bus/pci/devices/" + m_bdf;
    m_resourcePath = basePath + "/resource" + std::to_string(bar);

    FILE* f = std::fopen((basePath + "/resource").c_str(), "r");
    if (!f) {
        std::fprintf(stderr, "[PCIe] Cannot open %s/resource\n", basePath.c_str());
        m_bdf.clear();
        return false;
    }

    std::uint64_t start{}, end{}, flags{};
    for (int i = 0; i <= bar; i++) {
        if (std::fscanf(f, "%" SCNx64 " %" SCNx64 " %" SCNx64, &start, &end, &flags) != 3) {
            std::fprintf(stderr, "[PCIe] Cannot parse BAR%d for %s\n", bar, m_bdf.c_str());
            std::fclose(f);
            m_bdf.clear();
            return false;
        }
    }
    std::fclose(f);
    m_barSize = end - start + 1;
    return true;
}

bool InterfaceDiscovery::unbindDriver() {
    // Check if the driver is already unbound (e.g. from a previous run of the PMD)
    // by looking for the device's driver symlink in sysfs
    std::string driverLink = "/sys/bus/pci/devices/" + m_bdf + "/driver";
    if (::access(driverLink.c_str(), F_OK) != 0) {
        std::fprintf(stderr, "[PCIe] %s already unbound from %s (no driver symlink)\n",
                     m_driverName.c_str(), m_bdf.c_str());
        return true;
    }

    // Driver is still bound — attempt to unbind
    std::string unbindPath = "/sys/bus/pci/drivers/" + m_driverName + "/unbind";
    int fd = ::open(unbindPath.c_str(), O_WRONLY);
    if (fd < 0) {
        // Could not open unbind file — check if it got unbound between our two checks
        if (::access(driverLink.c_str(), F_OK) != 0) {
            std::fprintf(stderr, "[PCIe] %s unbound from %s (resolved after retry)\n",
                         m_driverName.c_str(), m_bdf.c_str());
            return true;
        }
        std::fprintf(stderr, "[PCIe] Cannot open %s: %s\n",
                     unbindPath.c_str(), std::strerror(errno));
        return false;
    }

    ssize_t written = ::write(fd, m_bdf.c_str(), m_bdf.size());
    ::close(fd);

    if (written < 0) {
        std::fprintf(stderr, "[PCIe] Unbind write failed for %s: %s\n",
                     m_bdf.c_str(), std::strerror(errno));
        return false;
    }

    // Verify the unbind actually took effect
    if (::access(driverLink.c_str(), F_OK) == 0) {
        std::fprintf(stderr, "[PCIe] Failed to unbind %s from %s (driver symlink still present)\n",
                     m_driverName.c_str(), m_bdf.c_str());
        return false;
    }

    std::fprintf(stderr, "[PCIe] Unbound %s from %s\n", m_driverName.c_str(), m_bdf.c_str());
    return true;
}

bool InterfaceDiscovery::enableBusMaster() {
    std::string configPath = "/sys/bus/pci/devices/" + m_bdf + "/config";
    int fd = ::open(configPath.c_str(), O_RDWR);
    if (fd < 0) {
        std::fprintf(stderr, "[PCIe] Cannot open PCI config: %s\n", configPath.c_str());
        return false;
    }

    // PCI Command Register at offset 0x04, 2 bytes
    std::uint16_t cmd{};
    if (::pread(fd, &cmd, sizeof(cmd), 0x04) != sizeof(cmd)) {
        std::fprintf(stderr, "[PCIe] Failed to read PCI command register\n");
        ::close(fd);
        return false;
    }

    cmd |= (1U << 2);  // Bus Master Enable

    if (::pwrite(fd, &cmd, sizeof(cmd), 0x04) != sizeof(cmd)) {
        std::fprintf(stderr, "[PCIe] Failed to write PCI command register\n");
        ::close(fd);
        return false;
    }

    ::close(fd);
    std::fprintf(stderr, "[PCIe] Bus mastering enabled for %s\n", m_bdf.c_str());
    return true;
}
