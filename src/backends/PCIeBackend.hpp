// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Areeb Sherjil
//
// Created by asherjil on 2/1/26.
//

#ifndef ABTEDGE_PCIEBACKEND_HPP
#define ABTEDGE_PCIEBACKEND_HPP

#include "BackendBase.hpp"
#include "common/BackendConcept.hpp"
#include "common/PCIeDiscoveryConcept.hpp"

#include <utility>

/// PCIe backend parametrised by a discovery policy.
///
/// The policy carries device-specific state and lifecycle hooks:
///   - VendorDeviceDiscovery : find by vendor/device ID (FPGAs, CPLDs, WRENs)
///   - InterfaceDiscovery    : find by Linux interface name, unbind driver (NICs)
///
/// PCIeBackend itself is a thin shell — all discovery and teardown logic
/// lives in the policy, keeping the backend zero-cost for policies that
/// don't need it (e.g. VendorDeviceDiscovery::release() is empty).
template<PCIeDiscovery Policy>
class PCIeBackend : public BackendBase {
public:
    template<typename... Args>
    explicit PCIeBackend(Args&&... args)
        : m_policy(std::forward<Args>(args)...) {}

    PCIeBackend(const PCIeBackend&) = delete;
    PCIeBackend& operator=(const PCIeBackend&) = delete;
    PCIeBackend(PCIeBackend&&) noexcept = default;

    PCIeBackend& operator=(PCIeBackend&& other) noexcept {
        if (this != &other) {
            BackendBase::close();
            m_policy.release();
            BackendBase::operator=(std::move(other));
            m_policy = std::move(other.m_policy);
        }
        return *this;
    }

    ~PCIeBackend() {
        BackendBase::close();
        m_policy.release();
    }

    [[nodiscard]] bool open() {
        if (!m_policy.prepare()) return false;
        return BackendBase::open(m_policy.resourcePath(), 0, m_policy.barSize());
    }

    Policy&       policy()       noexcept { return m_policy; }
    const Policy& policy() const noexcept { return m_policy; }

private:
    Policy m_policy;
};

#endif //ABTEDGE_PCIEBACKEND_HPP
