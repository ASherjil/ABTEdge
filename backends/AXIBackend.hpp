//
// Created by asherjil on 2/1/26.
//

#ifndef ABTEDGE_AXIBACKEND_H
#define ABTEDGE_AXIBACKEND_H

#include "Backend.hpp"
#include "common/BackendConcepts.hpp"

// This is to ensure all the previous memory access complete before continuing
#define FPGA_ARM_DATA_SYNC_BARRIER () __asm__ __volatile__("dsb sy" ::: "memory")

class AXIBackend : public Backend {
public:
    // Rule of 5 implementation
    AXIBackend() = default;
    AXIBackend(const AXIBackend&) = delete;
    AXIBackend(AXIBackend&&) noexcept = default; // call the base move constructor
    AXIBackend& operator=(const AXIBackend&) = delete;
    AXIBackend& operator=(AXIBackend&&) noexcept = default; // calls the base move assignment operator

    // 32-bit operations
    [[nodiscard]] std::uint32_t read32(std::size_t offset) const noexcept;
    [[nodiscard]] std::uint32_t read32FromField(std::size_t offset, std::uint32_t mask) const noexcept;
    void write32(std::size_t offset, std::uint32_t value) noexcept;
    void write32FromField(std::size_t offset, std::uint32_t value, std::uint32_t mask) noexcept;

    // 64-bit operations
    [[nodiscard]] std::uint64_t read64(std::size_t offset) const noexcept;
    [[nodiscard]] std::uint64_t read64FromField(std::size_t offset, std::uint64_t mask) const noexcept;
    void write64(std::size_t offset, std::uint64_t value) noexcept;
    void write64FromField(std::size_t offset, std::uint64_t value, std::uint64_t mask) noexcept;
};

// must check if the concept is satified
static_assert(FPGABackend<AXIBackend>, "AXIBackend must statify the FPGABackend concept");

#endif //ABTEDGE_AXIBACKEND_H