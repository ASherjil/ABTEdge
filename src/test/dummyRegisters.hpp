#ifndef FPGA_DUMMY_HW_DESC_REGISTERS_HPP
#define FPGA_DUMMY_HW_DESC_REGISTERS_HPP

// =============================================================================
// AUTO-GENERATED from dummy_hw_desc.csv — DO NOT EDIT
// Generated: 2026-02-05 08:52:05
// EDGE version: 4.1
// Device: dummy (Top level cheby for a dummy gateware to test registers; will be used for edge)
// =============================================================================

#include <cstdint>

namespace fpga::regs::dummy_hw_desc {

// --- Block: dummy ---

// Register offsets
constexpr std::size_t READ_ONLY_REG  = 0x0;  // [RO] uint32_t — RO test reg.
constexpr std::size_t READ_WRITE_REG = 0x4;  // [RW] uint32_t — RW test reg.
constexpr std::size_t MEAS_JITTER    = 0x8;  // [RW] uint32_t — RW reg to measure central FEC jitter.
constexpr std::size_t PLAY_GROUND    = 0xc;  // [RO] uint32_t — RW test reg w/ fields
constexpr std::size_t PG_1           = 0x10;  // [WO] uint8_t — write 1st byte of play_ground reg
constexpr std::size_t PG_2           = 0x14;  // [WO] uint8_t — write 2nd byte of play_ground reg
constexpr std::size_t PG_3           = 0x18;  // [RW] uint8_t — read/write 3rd byte of play_ground reg
constexpr std::size_t PG_4           = 0x1c;  // [RW] uint8_t — read/write 4th byte of play_ground reg

// Field masks for PLAY_GROUND (offset 0xc)
namespace play_ground {
    constexpr std::uint32_t PLAY_GROUND_BYTE_1 = 0x000000ff;  // [RO] 1th byte of play_ground reg
    constexpr std::uint32_t PLAY_GROUND_BYTE_2 = 0x0000ff00;  // [RO] 2nd byte of play_ground reg
    constexpr std::uint32_t PLAY_GROUND_BYTE_3 = 0x00ff0000;  // [RO] 3rd byte of play_ground reg
    constexpr std::uint32_t PLAY_GROUND_BYTE_4 = 0xff000000;  // [RO] 4th byte of play_ground reg
} // namespace play_ground

// --- Block Instances ---

constexpr std::size_t DUMMY_REGS_BASE = 0x0;  // used for hdl generation

} // namespace fpga::regs::dummy_hw_desc

#endif // FPGA_DUMMY_HW_DESC_REGISTERS_HPP
