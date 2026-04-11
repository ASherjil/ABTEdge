//
// Created by asherjil on 4/11/26.
//

#include "InterfaceDiscovery.hpp"

#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <unistd.h>
#include <utility>

namespace fs = std::filesystem;

InterfaceDiscovery::InterfaceDiscovery(std::string_view ifname, int bar) {
    std::error_code ec;
    std::string sysPath = "/sys/class/net/" + std::string(ifname) + "/device";

    // 1. Resolve BDF from interface symlink
    //    /sys/class/net/eno2/device -> ../../0000:0c:00.0
    auto devLink = fs::read_symlink(sysPath, ec);
    if (ec) {
        std::fprintf(stderr, "[PCIe] Cannot resolve device for interface %.*s: %s\n",
                     static_cast<int>(ifname.size()), ifname.data(), ec.message().c_str());
        return;
    }
    m_bdf = devLink.filename().string();

    // 2. Resolve bound driver name (may not be bound)
    //    /sys/class/net/eno2/device/driver -> ../../../../bus/pci/drivers/igb
    auto drvLink = fs::read_symlink(sysPath + "/driver", ec);
    if (!ec) {
        m_driverName = drvLink.filename().string();
    }

    // 3. Read BAR size from resource file
    std::string basePath = "/sys/bus/pci/devices/" + m_bdf;
    m_resourcePath = basePath + "/resource" + std::to_string(bar);

    FILE* f = std::fopen((basePath + "/resource").c_str(), "r");
    if (!f) {
        std::fprintf(stderr, "[PCIe] Cannot open %s/resource\n", basePath.c_str());
        m_bdf.clear();
        return;
    }

    std::uint64_t start{}, end{}, flags{};
    for (int i = 0; i <= bar; i++) {
        if (std::fscanf(f, "%" SCNx64 " %" SCNx64 " %" SCNx64, &start, &end, &flags) != 3) {
            std::fprintf(stderr, "[PCIe] Cannot parse BAR%d for %s\n", bar, m_bdf.c_str());
            std::fclose(f);
            m_bdf.clear();
            return;
        }
    }
    std::fclose(f);
    m_barSize = end - start + 1;

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
    if (m_driverName.empty() || m_bdf.empty()) return;

    std::fprintf(stderr, "[PCIe] Driver '%s' was unbound from %s.\n"
                         "[PCIe] To rebind: echo '%s' > /sys/bus/pci/drivers/%s/bind\n",
                 m_driverName.c_str(), m_bdf.c_str(),
                 m_bdf.c_str(), m_driverName.c_str());
}

std::string_view InterfaceDiscovery::bdf() const {
    return m_bdf;
}

std::string_view InterfaceDiscovery::driverName() const {
    return m_driverName;
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
