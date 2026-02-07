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
            std::printf("Connection failed, skipping test\n");
            return;
        }

        // Timed reads of all safe registers
        constexpr int numReads = 6;

        auto t0 = std::chrono::steady_clock::now();
        std::uint32_t v0 = *m_axiHandler.registerPtr<std::uint32_t>(READ_ONLY_REG);
        std::uint32_t v1 = *m_axiHandler.registerPtr<std::uint32_t>(READ_WRITE_REG);
        std::uint32_t v2 = *m_axiHandler.registerPtr<std::uint32_t>(MEAS_JITTER);
        std::uint32_t v3 = *m_axiHandler.registerPtr<std::uint32_t>(PLAY_GROUND);
        std::uint32_t v4 = *m_axiHandler.registerPtr<std::uint32_t>(PG_3);
        std::uint32_t v5 = *m_axiHandler.registerPtr<std::uint32_t>(PG_4);
        auto t1 = std::chrono::steady_clock::now();

        auto totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

        std::printf("\n=== AXI Register Dump ===\n");
        std::printf("  READ_ONLY_REG:  0x%08X\n", v0);
        std::printf("  READ_WRITE_REG: 0x%08X\n", v1);
        std::printf("  MEAS_JITTER:    0x%08X\n", v2);
        std::printf("  PLAY_GROUND:    0x%08X\n", v3);
        std::printf("  PG_3:           0x%08X\n", v4);
        std::printf("  PG_4:           0x%08X\n", v5);
        std::printf("  %ld ns total / %d reads = %ld ns avg\n", totalNs, numReads, totalNs / numReads);

        // Timed write + readback
        std::uint32_t testVal = 0xCEEDBEEF;

        auto writeStart = std::chrono::steady_clock::now();
        *m_axiHandler.registerPtr<std::uint32_t>(READ_WRITE_REG) = testVal;
        auto writeEnd = std::chrono::steady_clock::now();

        auto readStart = std::chrono::steady_clock::now();
        std::uint32_t readBack = *m_axiHandler.registerPtr<std::uint32_t>(READ_WRITE_REG);
        auto readEnd = std::chrono::steady_clock::now();

        auto writeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(writeEnd - writeStart).count();
        auto readNs  = std::chrono::duration_cast<std::chrono::nanoseconds>(readEnd - readStart).count();

        std::printf("\n=== AXI Write + Readback ===\n");
        std::printf("  Write 0x%08X | %ld ns\n", testVal, writeNs);
        std::printf("  Read  0x%08X | %ld ns | %s\n", readBack, readNs,
                    readBack == testVal ? "PASS" : "FAIL");
    }

    void generateSquarePulse() {
        using namespace fpga::regs::dummy_hw_desc;
        using namespace std::chrono_literals;
        if (!m_connectionResult) {
            std::printf("Connection failed, skipping test\n");
            return;
        }

        std::printf("\n=== Square Pulse Test (Ctrl+C to stop) ===\n");
        std::printf("Writing 1->0 to MEAS_JITTER every 1 second\n\n");

        auto* jitterReg = m_axiHandler.registerPtr<std::uint32_t>(MEAS_JITTER);
        while (true) {
            auto t0 = std::chrono::steady_clock::now();
            *jitterReg = 1;
            *jitterReg = 0;
            auto t1 = std::chrono::steady_clock::now();

            auto pulseNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
            std::printf("Pulse sent | 2 writes: %ld ns\n", pulseNs);
            std::this_thread::sleep_for(1000ms);
        }
    }
private:
    AXIBackend m_axiHandler;
    bool m_connectionResult{}; // set to false by default construction
};


#endif // FPGA_DUMMY_HW_DESC_REGISTERS_HPP
