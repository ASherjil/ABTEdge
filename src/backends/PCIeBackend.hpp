//
// Created by asherjil on 2/1/26.
//

#ifndef ABTEDGE_PCIEBACKEND_H
#define ABTEDGE_PCIEBACKEND_H

#include "Backend.hpp"
#include "common/BackendConcept.hpp"

class PCIeBackend : public BackendBase {
public:
    // Rule of 5 implementation
    PCIeBackend() = default;
    PCIeBackend(const PCIeBackend&) = delete;
    PCIeBackend(PCIeBackend&&) noexcept = default; // call the base move constructor
    PCIeBackend& operator=(const PCIeBackend&) = delete;
    PCIeBackend& operator=(PCIeBackend&&) noexcept = default; // calls the base move assignment operator

	// Future: x86-specific barrier if ever needed
};

// must check if the concept is satified
static_assert(FPGABackend<PCIeBackend>, "PCIeBackend must statify the FPGABackend concept");

#endif //ABTEDGE_PCIEBACKEND_H