// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Areeb Sherjil
//
// Created by asherjil on 4/4/26.
//

#ifndef ABTEDGE_DMARING_H
#define ABTEDGE_DMARING_H

#include "HugepageBuffer.hpp"
#include <cstdio>
#include <type_traits>
#include <utility>

template <typename Descriptor>
class DMARing {
  static_assert(std::is_trivially_copyable_v<Descriptor>, "DMA Descriptors must be trivially copyable");
public:
  DMARing () = default;
  DMARing(const DMARing &) = delete;
  DMARing(DMARing&& other) noexcept
    : m_buffer{std::move(other.m_buffer)},
      m_count{std::exchange(other.m_count, 0)} {}

  DMARing& operator=(const DMARing&) = delete;
  DMARing& operator=(DMARing&& other) noexcept {
    if (this != &other) {
      m_buffer = std::move(other.m_buffer);
      m_count  = std::exchange(other.m_count, 0);
    }
    return *this;
  }

  ~DMARing() = default; // HugepageBuffer destructor is enough

  [[nodiscard]] bool allocate(std::size_t descriptorCount) {
    if (m_count > 0) {
      std::fprintf(stderr, "[Error] Unable to allocate DMA, it is already allocated.\n");
      return false;
    }

    std::size_t ringBytes = descriptorCount * sizeof(Descriptor);
    if (!m_buffer.allocate(ringBytes)) {
      std::fprintf(stderr, "[Error] DMA memory allocation failure.\n");
      return false;
    }

    m_count = descriptorCount;
    return true;
  }

  void deallocate() {
    m_buffer.deallocate();
    m_count = 0;
  }

  // The real implementation of the index operator. Use the "Const-and-Back-Again" idiom.
  [[nodiscard]] const Descriptor& operator[](std::size_t index) const noexcept {
    return static_cast<const Descriptor*>(m_buffer.virtualAddr())[index];
  }

  [[nodiscard]] Descriptor& operator[](std::size_t index) noexcept {
    return const_cast<Descriptor&>(std::as_const(*this)[index]);
  }

  // Physical base address - write this to the device's descriptor base register
  [[nodiscard]] std::uint64_t physicalBase() const noexcept {
    return m_buffer.physicalAddr();
  }

  [[nodiscard]] std::size_t count() const noexcept {
    return m_count;
  }

  [[nodiscard]] std::size_t sizeBytes() const noexcept {
    return m_count * sizeof(Descriptor);
  }

  [[nodiscard]] bool isAllocated() const noexcept {
    return m_buffer.isAllocated();
  }

  [[nodiscard]] void* getHugepageBuffer() const noexcept {
    return m_buffer.virtualAddr();
  }
private:
  HugepageBuffer m_buffer;
  std::size_t m_count{};
};



#endif //ABTEDGE_DMARING_H
