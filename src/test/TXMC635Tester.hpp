//
// Created by asherjil on 2/6/26.
//

#ifndef FPGA_DRIVER_TXMC635TESTER_HHP_H
#define FPGA_DRIVER_TXMC635TESTER_HHP_H

#include "backends/PCIeBackend.hpp"
#include "backends/VendorDeviceDiscovery.hpp"
#include <cstdio>
#include <chrono>
#include <thread>

// === Memory Layout ===
constexpr std::uint32_t CORE_STRIDE       = 0x1000;      // 4KB per core
constexpr std::uint32_t CHANNEL_STRIDE    = 0x04;        // 4 bytes per channel
constexpr std::size_t   BAR0_SIZE         = 16*1024*1024;

// === ADC (ADAS3022) ===
constexpr std::uint32_t ADC_CORE_1_OFFSET = 0x02000;
constexpr std::uint32_t ADC_CORE_2_OFFSET = 0x03000;
constexpr std::uint32_t ADC_CORE_3_OFFSET = 0x04000;
constexpr std::uint32_t ADC_CORE_4_OFFSET = 0x05000;
constexpr std::uint32_t ADC_CFG_10V24     = 0x8CCF;      // PGIA=001 → ±10.24V, 312.5µV/LSB
constexpr std::uint32_t ADC_CFG_20V48     = 0x8FCF;      // PGIA=111 → ±20.48V (chip default after reset)
constexpr std::uint32_t ADC_CFG_DEFAULT   = ADC_CFG_10V24; // What CHWLib uses
constexpr std::uint32_t ADC_ACQ_MASK      = 0xFFFF0000;  // Bits [31:16]
constexpr std::uint32_t ADC_ACQ_SHIFT     = 16;
constexpr std::uint32_t ADC_CFG_MASK      = 0x0000FFFF;  // Bits [15:0]
constexpr std::uint32_t ADC_CH_IDX_MASK   = 0x00007000;  // Bits [14:12] in CFG
constexpr std::uint32_t ADC_CH_IDX_SHIFT  = 12;
constexpr std::uint32_t ADC_CONFIG_OFFSET = 0x20;
// PGIA[9:7] gain table: 000=±24.576V, 001=±10.24V, 010=±5.12V,
//   011=±2.56V, 100=±1.28V, 101=±0.64V, 111=±20.48V(default)

// === DAC (AD5764) ===
constexpr std::uint32_t DAC_CORE_1_OFFSET = 0x07000;     // Core 7 = AD5764 #0 (ch 0-3, DAC_1..DAC_4)
constexpr std::uint32_t DAC_CORE_2_OFFSET = 0x08000;     // Core 8 = AD5764 #1 (ch 4-7, DAC_5..DAC_8)
constexpr std::uint32_t DAC_WRITE_CMD     = 0x00100000;  // OR with 16-bit value
constexpr std::uint32_t DAC_COARSE_GAIN   = 0x001C0000;  // ±10V bipolar
constexpr std::uint32_t DAC_VALUE_MASK    = 0x0000FFFF;
constexpr std::uint32_t DAC_CORE_ID       = 0xBAD70803;
constexpr std::uint32_t DAC_RESET_OFFSET  = 0x38;
constexpr std::uint32_t DAC_COREID_OFFSET = 0x3C;

// === Reset Register (0x38) Bits ===
constexpr std::uint32_t DAC_RESET_RST     = (1 << 0);    // Hardware reset
constexpr std::uint32_t DAC_RESET_CLR     = (1 << 1);    // Clear
constexpr std::uint32_t DAC_RESET_SOFT    = (1 << 2);    // Soft reset

// TXMC635 FPGA vendor and device ID
constexpr std::uint16_t FPGA_VENDOR_ID  = 0xBAD7;
constexpr std::uint16_t FPGA_DEVICE_ID  = 0x7469;
constexpr int            FPGA_BAR       = 0; // BAR0 = cores, BAR1 = DMA RAM

// TXMC635 CPLD (MachXO2 carrier board) vendor and device ID
constexpr std::uint16_t CPLD_VENDOR_ID  = 0x1498;
constexpr std::uint16_t CPLD_DEVICE_ID  = 0x927b;
constexpr int            CPLD_BAR       = 0; // BAR0 = 256B config regs

// CPLD registers (BAR0) — safe to read
constexpr std::size_t CPLD_INT_ENABLE_OFFSET    = 0xC0; // RW - Interrupt enable
constexpr std::size_t CPLD_INT_STATUS_OFFSET    = 0xC4; // RW - Interrupt status
constexpr std::size_t CPLD_CONF_CONTROL_OFFSET  = 0xD0; // RW - Config control/status
constexpr std::size_t CPLD_CONF_DATA_OFFSET     = 0xD4; // RW - Config data
constexpr std::size_t CPLD_ISP_STATUS_OFFSET    = 0xEC; // RO - ISP status
constexpr std::size_t CPLD_IO_PULL_CFG_OFFSET   = 0xF4; // RW - IO pull resistor config
constexpr std::size_t CPLD_SERIAL_NUMBER_OFFSET = 0xF8; // RO - Board serial number
constexpr std::size_t CPLD_CODE_VERSION_OFFSET  = 0xFC; // RO - MachXO2 firmware version
// WARNING: offsets 0xE0, 0xE4, 0xE8 are ISP registers — DO NOT read/write (can brick CPLD)

class TXMC635Tester {
public:
    TXMC635Tester()
        : m_fpgaHandler{FPGA_VENDOR_ID, FPGA_DEVICE_ID, FPGA_BAR},
          m_cpldHandler{CPLD_VENDOR_ID, CPLD_DEVICE_ID, CPLD_BAR} {

        m_fpgaConnected = m_fpgaHandler.open();
        std::fprintf(stderr, "DEBUG: FPGA open=%d base=%p size=%zu\n",
                   m_fpgaConnected, m_fpgaHandler.getBaseAddress(),
                   m_fpgaHandler.getMmapSize());

        m_cpldConnected = m_cpldHandler.open();
        std::fprintf(stderr, "DEBUG: CPLD open=%d base=%p size=%zu\n",
                   m_cpldConnected, m_cpldHandler.getBaseAddress(),
                   m_cpldHandler.getMmapSize());
    }

    void performCPLDLatencyTest() {
        if (!m_cpldConnected) {
            std::printf("CPLD connection failed, skipping test\n");
            return;
        }

        constexpr int numReads = 8;

        auto t0 = std::chrono::steady_clock::now();
        std::uint32_t v0 = *m_cpldHandler.registerPtr<std::uint32_t>(CPLD_INT_ENABLE_OFFSET);
        std::uint32_t v1 = *m_cpldHandler.registerPtr<std::uint32_t>(CPLD_INT_STATUS_OFFSET);
        std::uint32_t v2 = *m_cpldHandler.registerPtr<std::uint32_t>(CPLD_CONF_CONTROL_OFFSET);
        std::uint32_t v3 = *m_cpldHandler.registerPtr<std::uint32_t>(CPLD_CONF_DATA_OFFSET);
        std::uint32_t v4 = *m_cpldHandler.registerPtr<std::uint32_t>(CPLD_ISP_STATUS_OFFSET);
        std::uint32_t v5 = *m_cpldHandler.registerPtr<std::uint32_t>(CPLD_IO_PULL_CFG_OFFSET);
        std::uint32_t v6 = *m_cpldHandler.registerPtr<std::uint32_t>(CPLD_SERIAL_NUMBER_OFFSET);
        std::uint32_t v7 = *m_cpldHandler.registerPtr<std::uint32_t>(CPLD_CODE_VERSION_OFFSET);
        auto t1 = std::chrono::steady_clock::now();

        auto totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

        std::printf("\n=== CPLD Register Dump (pure PCIe, no SPI) ===\n");
        std::printf("  int_enable:    0x%08X\n", v0);
        std::printf("  int_status:    0x%08X\n", v1);
        std::printf("  conf_control:  0x%08X\n", v2);
        std::printf("  conf_data:     0x%08X\n", v3);
        std::printf("  isp_status:    0x%08X\n", v4);
        std::printf("  io_pull_cfg:   0x%08X\n", v5);
        std::printf("  serial_number: 0x%08X\n", v6);
        std::printf("  code_version:  0x%08X\n", v7);
        std::printf("  %ld ns total / %d reads = %ld ns avg\n\n", totalNs, numReads, totalNs / numReads);
    }

    void performADCAcquisition() {
        if (!m_fpgaConnected) {
            std::printf("FPGA connection failed, skipping test\n");
            return;
        }

        constexpr std::size_t adcConfigOffset = ADC_CORE_1_OFFSET + ADC_CONFIG_OFFSET;
        constexpr std::size_t dacCoreIdOffset = DAC_CORE_1_OFFSET + DAC_COREID_OFFSET;

        std::printf("=== FPGA Verification ===\n");
        std::printf("  ADC Config:  0x%08X\n", *m_fpgaHandler.registerPtr<std::uint32_t>(adcConfigOffset));
        std::printf("  DAC Core ID: 0x%08X (expect 0x%08X)\n",
                    *m_fpgaHandler.registerPtr<std::uint32_t>(dacCoreIdOffset), DAC_CORE_ID);

        // Write to DAC_1 (Core 7, channel 0)
        constexpr std::size_t dac1Offset = DAC_CORE_1_OFFSET;
        constexpr double targetVolts = 3.0;
        auto dacVal = static_cast<std::int16_t>((targetVolts / 10.0) * 32768.0);

        *m_fpgaHandler.registerPtr<std::uint32_t>(dac1Offset) = DAC_COARSE_GAIN;

        // Timed DAC write
        auto dacWriteStart = std::chrono::steady_clock::now();
        *m_fpgaHandler.registerPtr<std::uint32_t>(dac1Offset) = DAC_WRITE_CMD | static_cast<std::uint16_t>(dacVal);
        auto dacWriteEnd = std::chrono::steady_clock::now();

        // Timed DAC readback (verify what was written)
        auto dacReadStart = std::chrono::steady_clock::now();
        std::uint32_t dacReadback = *m_fpgaHandler.registerPtr<std::uint32_t>(dac1Offset);
        auto dacReadEnd = std::chrono::steady_clock::now();

        auto writeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(dacWriteEnd - dacWriteStart).count();
        auto readNs  = std::chrono::duration_cast<std::chrono::nanoseconds>(dacReadEnd - dacReadStart).count();

        std::printf("\n=== DAC Write + Readback ===\n");
        std::printf("  DAC_1 write: %.2f V (raw=0x%04X) | %ld ns\n",
                    targetVolts, static_cast<std::uint16_t>(dacVal), writeNs);
        std::printf("  DAC_1 read:  0x%08X | %ld ns\n", dacReadback, readNs);

        // Wait for DAC to settle
        std::this_thread::sleep_for(std::chrono::microseconds(100));

        // Read from ADC_IN2, ADC_IN3, ADC_IN4
        constexpr std::size_t adcIn2Offset = ADC_CORE_1_OFFSET + (1 * CHANNEL_STRIDE);
        constexpr std::size_t adcIn3Offset = ADC_CORE_1_OFFSET + (2 * CHANNEL_STRIDE);
        constexpr std::size_t adcIn4Offset = ADC_CORE_1_OFFSET + (3 * CHANNEL_STRIDE);

        // Prime ADC pipeline
        (void)*m_fpgaHandler.registerPtr<std::uint32_t>(adcIn2Offset);
        (void)*m_fpgaHandler.registerPtr<std::uint32_t>(adcIn3Offset);
        (void)*m_fpgaHandler.registerPtr<std::uint32_t>(adcIn4Offset);
        std::this_thread::sleep_for(std::chrono::microseconds(100));

        constexpr int numReads = 3;

        auto adcStart = std::chrono::steady_clock::now();
        std::uint32_t rawIn2 = *m_fpgaHandler.registerPtr<std::uint32_t>(adcIn2Offset);
        std::uint32_t rawIn3 = *m_fpgaHandler.registerPtr<std::uint32_t>(adcIn3Offset);
        std::uint32_t rawIn4 = *m_fpgaHandler.registerPtr<std::uint32_t>(adcIn4Offset);
        auto adcEnd = std::chrono::steady_clock::now();

        auto totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(adcEnd - adcStart).count();

        std::printf("\n=== ADC Reads (DAC_1 = %.2f V) ===\n", targetVolts);
        std::printf("  ADC_IN2: 0x%08X | %+.4f V  (50R)\n", rawIn2, (static_cast<std::int16_t>(rawIn2 >> 16) / 32768.0) * 10.24);
        std::printf("  ADC_IN3: 0x%08X | %+.4f V  (open)\n", rawIn3, (static_cast<std::int16_t>(rawIn3 >> 16) / 32768.0) * 10.24);
        std::printf("  ADC_IN4: 0x%08X | %+.4f V  (50R)\n", rawIn4, (static_cast<std::int16_t>(rawIn4 >> 16) / 32768.0) * 10.24);
        std::printf("  %ld ns total / %d reads = %ld ns avg\n\n", totalNs, numReads, totalNs / numReads);
    }

private:
    PCIeBackend<VendorDeviceDiscovery> m_fpgaHandler;
    PCIeBackend<VendorDeviceDiscovery> m_cpldHandler;
    bool m_fpgaConnected{};
    bool m_cpldConnected{};
};

#endif //FPGA_DRIVER_TXMC635TESTER_HHP_H
