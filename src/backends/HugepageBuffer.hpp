// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Areeb Sherjil
//
// Created by asherjil on 4/4/26.
//

#ifndef ABTEDGE_HUGEPAGEBUFFER_H
#define ABTEDGE_HUGEPAGEBUFFER_H


#include <cstddef>
#include <cstdint>

// Default hugepage size on x86_64 and arm64 2MB
constexpr std::size_t HUGEPAGE_SIZE = 2 * 1024 * 1024;

class HugepageBuffer {
public:
  // Rule of five
  HugepageBuffer() = default;
  HugepageBuffer(const HugepageBuffer&) = delete;
  HugepageBuffer(HugepageBuffer&& other) noexcept;
  HugepageBuffer& operator=(const HugepageBuffer&) = delete;
  HugepageBuffer& operator=(HugepageBuffer&& other) noexcept;
  ~HugepageBuffer();

  [[nodiscard]] bool allocate(std::size_t);

  void deallocate();

  [[nodiscard]] bool isAllocated() const noexcept;
  [[nodiscard]] void* virtualAddr()  const noexcept;
  [[nodiscard]] std::uint64_t physicalAddr() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] std::uint64_t physicalAddrAt(std::size_t offset) const noexcept;

  template<typename T>
  [[nodiscard]] T* ptrAt(std::size_t offset) {
    return reinterpret_cast<T*>(static_cast<std::uint8_t*>(m_address) + offset);
  }
private:
  void* m_address{nullptr};
  std::uint64_t m_physicalAddress{};
  std::size_t m_size{};
};



#endif //ABTEDGE_HUGEPAGEBUFFER_H
