//
// Created by asherjil on 1/31/26.
//

#ifndef ABTEDGE_BACKENDCONCEPT_HPP
#define ABTEDGE_BACKENDCONCEPT_HPP

#include <cstdint>
#include <concepts>

template<typename T>
concept FPGABackend = requires(
    T backend,
    const T constBackend,
    const char* resourceName, // carefull not to use std::uint8_t*
    std::size_t size,
    std::size_t offset,
    std::uint32_t valueToWrite32,
    std::uint32_t mask32,
    std::uint64_t valueToWrite64,
    std::uint64_t mask64
)
{
    // Lifecyle
    { backend.open(resourceName, size) } -> std::same_as<bool>;
    { backend.close() } -> std::same_as<void>;

    // 32-bit register read/write and with mask
    { constBackend.read32(offset) } -> std::same_as<std::uint32_t>;
    { constBackend.read32FromField(offset, mask32) } -> std::same_as<std::uint32_t>;
    { backend.write32(offset, valueToWrite32) } -> std::same_as<void>;
    { backend.write32ToField(offset, mask32, valueToWrite32) } -> std::same_as<void>;

    // 64-bit regiser read/write and with mask
    { constBackend.read64(offset) } -> std::same_as<std::uint64_t>;
    { constBackend.read64FromField(offset, mask64) } -> std::same_as<std::uint64_t>;
    { backend.write64(offset, valueToWrite64) } -> std::same_as<void>;
    { backend.write64ToField(offset, mask64, valueToWrite64) } -> std::same_as<void>;

    // Status
    { constBackend.isOpen() } -> std::same_as<bool>;
    { constBackend.getBaseAddress() } -> std::same_as<void*>;
    { constBackend.getMmapSize() } -> std::same_as<std::size_t>;
};

#endif //ABTEDGE_BACKENDCONCEPT_HPP