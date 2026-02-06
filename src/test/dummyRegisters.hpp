#ifndef FPGA_DUMMY_HW_DESC_REGISTERS_HPP
#define FPGA_DUMMY_HW_DESC_REGISTERS_HPP

// =============================================================================
// AUTO-GENERATED from dummy_hw_desc.csv — DO NOT EDIT
// Generated: 2026-02-05 08:52:05
// EDGE version: 4.1
// Device: dummy (Top level cheby for a dummy gateware to test registers; will be used for edge)
// =============================================================================
#include "backends/AXIBackend.hpp"
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <thread>

namespace fpga::regs::dummy_hw_desc {

// --- Block: dummy ---
// found by running "ls /sys/bus/platform/devices/ | grep -i dummy" on the DIOT
constexpr std::size_t DUMMY_PHYSICAL_ADDRESS = 0xB0020000; // found from the device tree
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


class AXIBusTester {
public:
    AXIBusTester() {
        using namespace fpga::regs::dummy_hw_desc;
        std::printf("Opening /dev/mem at physical address 0x%zX...\n", DUMMY_PHYSICAL_ADDRESS);

        if (!m_axiHandler.open("/dev/mem", DUMMY_PHYSICAL_ADDRESS, 0x1000 /*4KB is enough*/)) {
            std::perror("Failed to open /dev/mem");
            return;
        }
        m_connectionResult = true; //connection succeeded
    }

    void performReadWriteTest() {
        using namespace fpga::regs::dummy_hw_desc;
        if (!m_connectionResult) {
            std::printf("Connection result failure cannot perform any tests");
            return;
        }
        // Read all registers and print their values
        std::printf("=== Reading All Registers ===\n\n");

        // READ_ONLY_REG (0x0) - RO
        auto readOnly = *m_axiHandler.registerPtr<std::uint32_t>(READ_ONLY_REG);
        std::printf("READ_ONLY_REG  (0x%02zX): 0x%08X\n", READ_ONLY_REG, readOnly);

        // READ_WRITE_REG (0x4) - RW
        auto readWrite = *m_axiHandler.registerPtr<std::uint32_t>(READ_WRITE_REG);
        std::printf("READ_WRITE_REG (0x%02zX): 0x%08X\n", READ_WRITE_REG, readWrite);

        // MEAS_JITTER (0x8) - RW
        auto measJitter = *m_axiHandler.registerPtr<std::uint32_t>(MEAS_JITTER);
        std::printf("MEAS_JITTER    (0x%02zX): 0x%08X\n", MEAS_JITTER, measJitter);

        // PLAY_GROUND (0xc) - RO
        auto playGround = *m_axiHandler.registerPtr<std::uint32_t>(PLAY_GROUND);
        std::printf("PLAY_GROUND    (0x%02zX): 0x%08X\n", PLAY_GROUND, playGround);

        // PG_3 (0x18) - RW
        auto pg3 = *m_axiHandler.registerPtr<std::uint8_t>(PG_3);
        std::printf("PG_3           (0x%02zX): 0x%02X\n", PG_3, pg3);

        // PG_4 (0x1c) - RW
        auto pg4 = *m_axiHandler.registerPtr<std::uint8_t>(PG_4);
        std::printf("PG_4           (0x%02zX): 0x%02X\n", PG_4, pg4);

        // Test write then read back
        std::printf("\n=== Write/Read Test ===\n\n");

        std::uint32_t testVal = 0xCEEDBEEF;
        std::printf("Writing 0x%08X to READ_WRITE_REG...\n", testVal);
        *m_axiHandler.registerPtr<std::uint32_t>(READ_WRITE_REG) = testVal;

        auto readBack = *m_axiHandler.registerPtr<std::uint32_t>(READ_WRITE_REG);
        std::printf("Read back: 0x%08X\n", readBack);

        if (readBack == testVal) {
            std::printf("SUCCESS: Write/read test passed!\n");
        } else {
            std::printf("FAILED: Expected 0x%08X, got 0x%08X\n", testVal, readBack);
        }

        // Dump first 32 bytes as raw hex
        std::printf("\n=== Raw Memory Dump (first 32 bytes) ===\n");
        auto* base = m_axiHandler.registerPtr<std::uint32_t>(0);
        for (int i = 0; i < 8; ++i) {
            std::printf("  [0x%02X]: 0x%08X\n", i * 4, base[i]);
        }
    }

    void generateSquarePulse() {
        using namespace fpga::regs::dummy_hw_desc;
        using namespace std::chrono_literals;
        if (!m_connectionResult) {
            std::printf("Connection result failure cannot perform any tests");
            return;
        }
        // Square pulse test for oscilloscope
        std::printf("\n=== Square Pulse Test (Ctrl+C to stop) ===\n");
        std::printf("Writing 1->0 to MEAS_JITTER every 1 second...\n");
        std::printf("Pulse width = write-to-write latency\n\n");

        auto* jitterReg = m_axiHandler.registerPtr<std::uint32_t>(MEAS_JITTER);
        while (true) {
            *jitterReg = 1;
            *jitterReg = 0;

            std::printf("Pulse sent!\n");
            std::this_thread::sleep_for(1000ms);
        }
    }
private:
    AXIBackend m_axiHandler;
    bool m_connectionResult{}; // set to false by default construction
};


#endif // FPGA_DUMMY_HW_DESC_REGISTERS_HPP
