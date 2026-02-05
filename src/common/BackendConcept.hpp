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
    std::size_t physicalAddress,
    std::uint32_t valueToWrite32,
    std::uint64_t valueToWrite64
)
{
    // Lifecyle
    { backend.open(resourceName, physicalAddress, size) } -> std::same_as<bool>;
    { backend.close() } -> std::same_as<void>;

	// Read/write of the registers
	{ constBackend.template registerPtr<std::uint32_t>(offset) } -> std::same_as<volatile std::uint32_t*>;
	{ constBackend.template registerPtr<std::uint64_t>(offset) } -> std::same_as<volatile std::uint64_t*>;
	{ constBackend.template readFromField<std::uint32_t{0xff}>(offset) } -> std::same_as<std::uint32_t>;
	{ constBackend.template readFromField<std::uint64_t{0xff}>(offset) } -> std::same_as<std::uint64_t>;
	{ backend.template writeToField<std::uint32_t{0xff}>(offset, valueToWrite32) } -> std::same_as<void>;
	{ backend.template writeToField<std::uint64_t{0xff}>(offset, valueToWrite64) } -> std::same_as<void>;

    // Status
    { constBackend.isOpen() } -> std::same_as<bool>;
    { constBackend.getBaseAddress() } -> std::same_as<void*>;
    { constBackend.getMmapSize() } -> std::same_as<std::size_t>;
};

#endif //ABTEDGE_BACKENDCONCEPT_HPP