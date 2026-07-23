// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Areeb Sherjil
//
// Created by asherjil on 4/11/26.
//

#ifndef ABTEDGE_PCIEDISCOVERYCONCEPT_HPP
#define ABTEDGE_PCIEDISCOVERYCONCEPT_HPP

#include <concepts>
#include <cstddef>

/// A PCIeDiscovery policy knows how to locate a PCI device, prepare it
/// for mmap (e.g. unbind a kernel driver), and release it afterwards.
template<typename T>
concept PCIeDiscovery = requires(T& policy, const T& constPolicy) {
    { policy.prepare() }           -> std::same_as<bool>;
    { constPolicy.resourcePath() } -> std::convertible_to<const char*>;
    { constPolicy.barSize() }      -> std::same_as<std::size_t>;
    { policy.release() }           -> std::same_as<void>;
};

#endif //ABTEDGE_PCIEDISCOVERYCONCEPT_HPP
