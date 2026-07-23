// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Areeb Sherjil
//
// Created by asherjil on 2/1/26.
//

#ifndef ABTEDGE_AXIBACKEND_H
#define ABTEDGE_AXIBACKEND_H

#include "BackendBase.hpp"
#include "common/BackendConcept.hpp"

class AXIBackend : public BackendBase {
public:
  // Rule of 5 implementation
  AXIBackend() = default;
  AXIBackend(const AXIBackend&) = delete;
  AXIBackend(AXIBackend&&) noexcept = default; // call the base move constructor
  AXIBackend& operator=(const AXIBackend&) = delete;
  AXIBackend& operator=(AXIBackend&&) noexcept = default; // calls the base move assignment operator
  // Future: ARM-specific barrier if ever needed
};

// must check if the concept is satified
static_assert(HardwareBus<AXIBackend>, "AXIBackend must statify the HardwareBus concept");

#endif //ABTEDGE_AXIBACKEND_H