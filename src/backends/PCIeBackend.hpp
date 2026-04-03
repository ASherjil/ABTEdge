//
// Created by asherjil on 2/1/26.
//

#ifndef ABTEDGE_PCIEBACKEND_H
#define ABTEDGE_PCIEBACKEND_H

#include "BackendBase.hpp"
#include "common/BackendConcept.hpp"
#include <filesystem>
#include <string_view>

constexpr std::string_view PCIE_DIRECTORY = "/sys/bus/pci/devices";

class PCIeBackend : public BackendBase {
public:
    // Rule of 5 implementation
    PCIeBackend(std::uint16_t vendorID, std::uint16_t deviceID, int bar);
    PCIeBackend(const PCIeBackend&) = delete;
    PCIeBackend(PCIeBackend&& other) noexcept;
    PCIeBackend& operator=(const PCIeBackend&) = delete;
    PCIeBackend& operator=(PCIeBackend&& other) noexcept;

    [[nodiscard]] bool open();
	// Future: x86-specific barrier if ever needed
private:
    std::filesystem::path m_pcieResourcePath{}; // find the BAR of the PCIe device, this is need to open the device
    std::size_t m_barSize{}; // this is the bar size PCIe resource
};

// must check if the concept is satified
static_assert(HardwareBus<PCIeBackend>, "PCIeBackend must statify the HardwareBus concept");

#endif //ABTEDGE_PCIEBACKEND_H