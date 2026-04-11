//
// Created by asherjil on 4/11/26.
//

#ifndef ABTEDGE_INTERFACEDISCOVERY_HPP
#define ABTEDGE_INTERFACEDISCOVERY_HPP

#include <cstddef>
#include <string>
#include <string_view>

/// Discovery policy: locate a PCI NIC by Linux network interface name.
/// Resolves BDF and driver from sysfs, unbinds the kernel driver on prepare(),
/// and re-enables bus mastering. On release() prints the rebind command for the user.
class InterfaceDiscovery {
public:
    // Rule of 5: move-only
    explicit InterfaceDiscovery(std::string_view ifname, int bar = 0);
    InterfaceDiscovery(const InterfaceDiscovery&) = delete;
    InterfaceDiscovery& operator=(const InterfaceDiscovery&) = delete;
    InterfaceDiscovery(InterfaceDiscovery&& other) noexcept;
    InterfaceDiscovery& operator=(InterfaceDiscovery&& other) noexcept;
    ~InterfaceDiscovery();

    // PCIeDiscovery concept interface
    [[nodiscard]] bool        prepare();
    [[nodiscard]] const char* resourcePath() const;
    [[nodiscard]] std::size_t barSize() const;
    void                      release();

    [[nodiscard]] std::string_view bdf() const;
    [[nodiscard]] std::string_view driverName() const;

private:
    std::string m_bdf{};
    std::string m_driverName{};
    std::string m_resourcePath{};
    std::size_t m_barSize{};

    bool unbindDriver();
    bool enableBusMaster();
};

#endif //ABTEDGE_INTERFACEDISCOVERY_HPP
