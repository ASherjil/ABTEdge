//
// Created by asherjil on 2/6/26.
//

#ifndef FPGA_DRIVER_TXMC635TESTER_HHP_H
#define FPGA_DRIVER_TXMC635TESTER_HHP_H

#include "backends/PCIeBackend.hpp"
#include <cstdio>
#include <chrono>

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

// TXMC635 vendor and device ID
constexpr std::uint16_t VENDOR_ID  = 0xBAD7;
constexpr std::uint16_t DEVICE_ID  = 0x7469;
constexpr int      BAR_NUMBER = 0; // BAR0 are the cores, whereas BAR1 is the DMA RAM.

class TXMC635Tester {
public:
    TXMC635Tester(std::uint16_t vendorID, std::uint16_t deviceID, int bar)
        :m_pcieHandler{vendorID, deviceID, bar} {
        m_connectionResult = m_pcieHandler.open(); // open the PCIe device
        std::fprintf(stderr, "DEBUG: open=%d base=%p size=%zu\n",
                   m_connectionResult, m_pcieHandler.getBaseAddress(),
                   m_pcieHandler.getMmapSize());
    }

    void performADCAcquisition() {
        if (!m_connectionResult) {
            std::printf("Connection to the TXMC635 failed, no tests can be performed\n");
            return;
        }

        constexpr std::size_t adcConfigOffset = ADC_CORE_1_OFFSET + ADC_CONFIG_OFFSET;
        std::uint32_t adcConfigValue = *m_pcieHandler.registerPtr<std::uint32_t>(adcConfigOffset);

        std::printf("ADC Config (0x%04zX): 0x%08X\n", adcConfigOffset, adcConfigValue);

        constexpr std::size_t adcChannel0Offset = ADC_CORE_1_OFFSET;

        auto start = std::chrono::steady_clock::now();
        std::uint32_t raw = *m_pcieHandler.registerPtr<std::uint32_t>(adcChannel0Offset);
        auto end = std::chrono::steady_clock::now();

        double volts = (static_cast<std::int16_t>(raw >> 16) / 32768.0) * 10.24;
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        std::printf("ADC CH0 raw: 0x%08X | Voltage: %.4f V | Read latency: %ld ns\n",
                    raw, volts, elapsed);
    }

private:
    PCIeBackend m_pcieHandler; // set in the constructor
    bool m_connectionResult{};
};

#endif //FPGA_DRIVER_TXMC635TESTER_HHP_H
