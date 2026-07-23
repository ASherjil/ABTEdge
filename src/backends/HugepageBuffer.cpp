// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Areeb Sherjil
//
// Created by asherjil on 4/4/26.
//
#include "common/HugePageHelpers.hpp"
#include "HugepageBuffer.hpp"
#include <cstring>
#include <utility>
#include <sys/mman.h>
#include <cstdio>

HugepageBuffer::HugepageBuffer(HugepageBuffer &&other) noexcept
  : m_address{std::exchange(other.m_address, nullptr)},
    m_physicalAddress{std::exchange(other.m_physicalAddress, 0)},
    m_size{std::exchange(other.m_size, 0)} {}

HugepageBuffer &HugepageBuffer::operator=(HugepageBuffer &&other) noexcept {
  if (this != &other) {
    deallocate();

    m_address         = std::exchange(other.m_address, nullptr);
    m_physicalAddress = std::exchange(other.m_physicalAddress, 0);
    m_size            = std::exchange(other.m_size, 0);
  }
  return *this;
}

HugepageBuffer::~HugepageBuffer() {
  deallocate();
}

bool HugepageBuffer::allocate(std::size_t size) {

  if (m_address) {
    return false;
  }

  m_size = (size + HUGEPAGE_SIZE - 1) & ~ (HUGEPAGE_SIZE - 1);
  m_address = ::mmap(
    nullptr,
    m_size,
    PROT_READ | PROT_WRITE,
    MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_LOCKED,
    -1,
    0);

  if (m_address == MAP_FAILED) {
    std::fprintf(stderr, "[Error] Hugepage mmap failed (size=%zu). "
                         "Check /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages\n", m_size);
    m_address = nullptr;
    m_size = 0;
    return false;
  }

  std::memset(m_address, 0, m_size);

  m_physicalAddress = resolvePhysicalAddress(m_address); // this is defined in the common/XXX directory
  if (m_physicalAddress == 0) {
    std::fprintf(stderr, "[Error] Failed to resolve physical address.\n");
    ::munmap(m_address, m_size);
    m_address = nullptr;
    m_size = 0;
    return false;
  }

  return true;
}

void HugepageBuffer::deallocate() {
  if (m_address) {
    ::munmap(m_address, m_size);
  }
  m_address = nullptr;
  m_physicalAddress = 0;
  m_size = 0;
}

bool HugepageBuffer::isAllocated() const noexcept {
  return m_address != nullptr;
}

void* HugepageBuffer::virtualAddr() const noexcept {
  return m_address;
}

std::uint64_t HugepageBuffer::physicalAddr() const noexcept {
  return m_physicalAddress;
}

std::size_t HugepageBuffer::size() const noexcept {
  return m_size;
}

std::uint64_t HugepageBuffer::physicalAddrAt(std::size_t offset) const noexcept {
  return m_physicalAddress + offset;
}
