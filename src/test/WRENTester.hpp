//
// Created by asherjil on 2/14/26.
//

#ifndef FPGA_DRIVER_WRENTESTER_HPP
#define FPGA_DRIVER_WRENTESTER_HPP
#include "backends/PCIeBackend.hpp"
#include <cstdint>
#include <cstdio>
#include <chrono>

// WREN PCI identification
constexpr std::uint16_t WREN_VENDOR_ID = 0x10DC; // CERN
constexpr std::uint16_t WREN_DEVICE_ID = 0x0455;
constexpr int WREN_BAR                 = 1;       // BAR1 = host registers + mailbox

// Host register offsets (BAR1 + 0x0000) — all safe to read
constexpr std::size_t WREN_IDENT          = 0x00; // "WREN" = 0x5745524E
constexpr std::size_t WREN_MAP_VERSION    = 0x04;
constexpr std::size_t WREN_MODEL_ID       = 0x08;
constexpr std::size_t WREN_FW_VERSION     = 0x0C;
constexpr std::size_t WREN_WR_STATE       = 0x10; // bit 0: link_up, bit 1: time_valid
constexpr std::size_t WREN_TM_TAI_LO      = 0x14; // Current TAI seconds (low 32b)
constexpr std::size_t WREN_TM_TAI_HI      = 0x18; // Current TAI seconds (high bits)
constexpr std::size_t WREN_TM_CYCLES      = 0x1C; // 28-bit cycle counter @ 62.5MHz (16ns/tick)
constexpr std::size_t WREN_TM_COMPACT     = 0x20; // Compact time: cycles[27:0] + tai_sec[31:28]
constexpr std::size_t WREN_ISR            = 0x28; // Interrupt status (masked)
constexpr std::size_t WREN_ISR_RAW        = 0x2C; // Raw interrupt status (read-only, safe)
constexpr std::size_t WREN_IMR            = 0x30; // Interrupt mask register
// WARNING: 0x34 = IACK (write-only) — DO NOT read or write

// Mailbox async ring buffer offsets (BAR1 + 0x10000 + ...)
constexpr std::size_t WREN_ASYNC_DATA_BASE  = 0x12000; // async_data[0..2047]
constexpr std::size_t WREN_ASYNC_BOARD_OFF  = 0x14000; // firmware write pointer
constexpr std::size_t WREN_ASYNC_HOST_OFF   = 0x14004; // driver read pointer — NEVER write

class WRENTester {
public:
    WRENTester()
        : m_pcieHandler{WREN_VENDOR_ID, WREN_DEVICE_ID, WREN_BAR} {
        m_connected = m_pcieHandler.open();

        if (!m_connected) {
            std::fprintf(stderr, "Connection failure, unable to open WREN PCIe device.\n");
            return;
        }

        std::fprintf(stderr, "DEBUG: WREN PCIe open=%d base=%p size=%zu\n",
                     m_connected, m_pcieHandler.getBaseAddress(),
                     m_pcieHandler.getMmapSize());
    }

    // Bulk read all safe host registers with timing
    // Uses 64-bit reads to pair adjacent registers (7 PCIe round-trips instead of 12)
    void performHostRegisterDump() {
        if (!m_connected) {
            std::printf("WREN connection failed, skipping test\n");
            return;
        }

        constexpr int NUM_TRANSACTIONS = 7; // 5x 64-bit + 2x 32-bit

        auto t0 = std::chrono::steady_clock::now();
        // 64-bit reads: pair adjacent registers into single PCIe transactions
        std::uint64_t identAndMap   = *m_pcieHandler.registerPtr<std::uint64_t>(WREN_IDENT);       // 0x00+0x04
        std::uint64_t modelAndFw    = *m_pcieHandler.registerPtr<std::uint64_t>(WREN_MODEL_ID);    // 0x08+0x0C
        std::uint64_t stateAndTaiLo = *m_pcieHandler.registerPtr<std::uint64_t>(WREN_WR_STATE);    // 0x10+0x14
        std::uint64_t taiHiAndCyc   = *m_pcieHandler.registerPtr<std::uint64_t>(WREN_TM_TAI_HI);  // 0x18+0x1C
        std::uint32_t compact       = *m_pcieHandler.registerPtr<std::uint32_t>(WREN_TM_COMPACT);  // 0x20 (gap to 0x28)
        std::uint64_t isrAndRaw     = *m_pcieHandler.registerPtr<std::uint64_t>(WREN_ISR);         // 0x28+0x2C
        std::uint32_t imr           = *m_pcieHandler.registerPtr<std::uint32_t>(WREN_IMR);         // 0x30
        auto t1 = std::chrono::steady_clock::now();

        auto totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

        // Little-endian: low 32 bits = lower address, high 32 bits = higher address
        auto ident      = static_cast<std::uint32_t>(identAndMap);
        auto mapVersion = static_cast<std::uint32_t>(identAndMap >> 32);
        auto modelId    = static_cast<std::uint32_t>(modelAndFw);
        auto fwVersion  = static_cast<std::uint32_t>(modelAndFw >> 32);
        auto wrState    = static_cast<std::uint32_t>(stateAndTaiLo);
        auto taiLo      = static_cast<std::uint32_t>(stateAndTaiLo >> 32);
        auto taiHi      = static_cast<std::uint32_t>(taiHiAndCyc);
        auto cycles     = static_cast<std::uint32_t>(taiHiAndCyc >> 32);
        auto isr        = static_cast<std::uint32_t>(isrAndRaw);
        auto isrRaw     = static_cast<std::uint32_t>(isrAndRaw >> 32);

        // Decode IDENT as ASCII string
        char identStr[5];
        identStr[0] = static_cast<char>((ident >> 24) & 0xFF);
        identStr[1] = static_cast<char>((ident >> 16) & 0xFF);
        identStr[2] = static_cast<char>((ident >>  8) & 0xFF);
        identStr[3] = static_cast<char>((ident >>  0) & 0xFF);
        identStr[4] = '\0';

        std::printf("\n=== WREN Host Register Dump (BAR1, %d transactions: 5x64-bit + 2x32-bit) ===\n", NUM_TRANSACTIONS);
        std::printf("  ident:        0x%08X  (\"%s\")\n", ident, identStr);
        std::printf("  map_version:  0x%08X\n", mapVersion);
        std::printf("  model_id:     0x%08X\n", modelId);
        std::printf("  fw_version:   0x%08X\n", fwVersion);
        std::printf("  wr_state:     0x%08X  (link=%s, time=%s)\n",
                    wrState,
                    (wrState & 0x1) ? "UP" : "DOWN",
                    (wrState & 0x2) ? "VALID" : "INVALID");
        std::printf("  tm_tai_lo:    0x%08X  (%u s)\n", taiLo, taiLo);
        std::printf("  tm_tai_hi:    0x%08X\n", taiHi);
        std::printf("  tm_cycles:    0x%08X  (%u ns)\n", cycles, (cycles & 0x0FFFFFFF) * 16);
        std::printf("  tm_compact:   0x%08X\n", compact);
        std::printf("  isr:          0x%08X\n", isr);
        std::printf("  isr_raw:      0x%08X\n", isrRaw);
        std::printf("  imr:          0x%08X\n", imr);
        std::printf("  ---\n");
        std::printf("  %ld ns total / %d PCIe transactions = %ld ns avg\n\n", totalNs, NUM_TRANSACTIONS, totalNs / NUM_TRANSACTIONS);
    }

    // Busy-poll the async ring buffer for CTIM/LTIM events with due times
    // Self-terminates after durationSeconds as a watchdog safety measure
    void pollTimingEvents(int durationSeconds = 30) {
        if (!m_connected) {
            std::printf("WREN connection failed, skipping test\n");
            return;
        }

        // Capsule header bit masks (word 0 of every capsule)
        constexpr std::uint32_t HDR_TYP_MASK = 0x000000FF; // bits [7:0]
        constexpr std::uint32_t HDR_LEN_MASK = 0xFFFF0000; // bits [31:16]
        constexpr unsigned      HDR_LEN_SHIFT = 16;
        constexpr std::uint8_t  TYP_EVENT    = 0x02;       // CMD_ASYNC_EVENT

        // Event capsule word 1 masks
        constexpr std::uint32_t EVT_ID_MASK   = 0x0000FFFF; // bits [15:0]
        constexpr std::uint32_t CTXT_ID_MASK  = 0xFFFF0000; // bits [31:16]
        constexpr unsigned      CTXT_ID_SHIFT = 16;

        constexpr std::uint32_t RING_MASK = 2047; // wrap at 2048 words

        std::printf("\n=== WREN Event Poll (watchdog: %ds) ===\n", durationSeconds);
        std::printf("  Listening for CTIM/LTIM events (typ=0x02)...\n");
        std::printf("  (Requires active wren-pcie driver with event subscriptions)\n\n");

        // Sync to current ring buffer position
        std::uint32_t shadowOff = *m_pcieHandler.registerPtr<std::uint32_t>(WREN_ASYNC_BOARD_OFF);
        std::printf("  Initial async_board_off: %u\n\n", shadowOff);

        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(durationSeconds);
        int eventsFound = 0;
        long totalPollNs = 0;
        long totalParseNs = 0;
        long pollCount = 0;

        while (std::chrono::steady_clock::now() < deadline) {
            auto tPoll = std::chrono::steady_clock::now();
            std::uint32_t boardOff = *m_pcieHandler.registerPtr<std::uint32_t>(WREN_ASYNC_BOARD_OFF);
            totalPollNs += std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - tPoll).count();
            ++pollCount;

            if (boardOff == shadowOff)
                continue;

            // Process all new capsules
            while (shadowOff != boardOff) {
                // Check if entire 4-word capsule fits without ring wrap
                bool noWrap = (shadowOff + 3) <= RING_MASK;

                if (noWrap) {
                    // Fast path: 2x 64-bit reads for entire capsule (2 PCIe round-trips)
                    auto tParse = std::chrono::steady_clock::now();

                    // 64-bit read #1: header (word 0) + ids (word 1)
                    std::uint64_t hdrAndIds = *m_pcieHandler.registerPtr<std::uint64_t>(
                        WREN_ASYNC_DATA_BASE + shadowOff * 4);
                    auto hdr = static_cast<std::uint32_t>(hdrAndIds);
                    std::uint8_t  typ = hdr & HDR_TYP_MASK;
                    std::uint16_t len = static_cast<std::uint16_t>((hdr & HDR_LEN_MASK) >> HDR_LEN_SHIFT);

                    if (len == 0 || len > 2048) {
                        std::printf("  [ERROR] Invalid capsule len=%u at offset=%u, aborting\n", len, shadowOff);
                        goto done;
                    }

                    if (typ == TYP_EVENT) {
                        // 64-bit read #2: nsec (word 2) + sec (word 3)
                        std::uint64_t nsecAndSec = *m_pcieHandler.registerPtr<std::uint64_t>(
                            WREN_ASYNC_DATA_BASE + (shadowOff + 2) * 4);

                        auto parseNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - tParse).count();
                        totalParseNs += parseNs;

                        auto ids  = static_cast<std::uint32_t>(hdrAndIds >> 32);
                        auto nsec = static_cast<std::uint32_t>(nsecAndSec);
                        auto sec  = static_cast<std::uint32_t>(nsecAndSec >> 32);
                        std::uint16_t evId   = ids & EVT_ID_MASK;
                        std::uint16_t ctxtId = static_cast<std::uint16_t>((ids & CTXT_ID_MASK) >> CTXT_ID_SHIFT);

                        std::printf("  #%-4d  EVENT  ev_id=%-5u  ctxt_id=%-3u  due_time=%u.%09u TAI  [%ld ns, 2x64]\n",
                                    ++eventsFound, evId, ctxtId, sec, nsec, parseNs);
                    }

                    shadowOff = (shadowOff + len) & RING_MASK;
                } else {
                    // Slow path: near ring boundary, use 32-bit reads with wrap masking
                    std::uint32_t hdr = readRingWord(shadowOff);
                    std::uint8_t  typ = hdr & HDR_TYP_MASK;
                    std::uint16_t len = static_cast<std::uint16_t>((hdr & HDR_LEN_MASK) >> HDR_LEN_SHIFT);

                    if (len == 0 || len > 2048) {
                        std::printf("  [ERROR] Invalid capsule len=%u at offset=%u, aborting\n", len, shadowOff);
                        goto done;
                    }

                    if (typ == TYP_EVENT) {
                        auto tParse = std::chrono::steady_clock::now();

                        std::uint32_t ids  = readRingWord((shadowOff + 1) & RING_MASK);
                        std::uint32_t nsec = readRingWord((shadowOff + 2) & RING_MASK);
                        std::uint32_t sec  = readRingWord((shadowOff + 3) & RING_MASK);

                        auto parseNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - tParse).count();
                        totalParseNs += parseNs;

                        std::uint16_t evId   = ids & EVT_ID_MASK;
                        std::uint16_t ctxtId = static_cast<std::uint16_t>((ids & CTXT_ID_MASK) >> CTXT_ID_SHIFT);

                        std::printf("  #%-4d  EVENT  ev_id=%-5u  ctxt_id=%-3u  due_time=%u.%09u TAI  [%ld ns, 32-bit]\n",
                                    ++eventsFound, evId, ctxtId, sec, nsec, parseNs);
                    }

                    shadowOff = (shadowOff + len) & RING_MASK;
                }
            }
        }

    done:
        std::printf("\n  --- Summary ---\n");
        std::printf("  Watchdog:           %d seconds\n", durationSeconds);
        std::printf("  Poll iterations:    %ld\n", pollCount);
        std::printf("  Events caught:      %d\n", eventsFound);
        if (pollCount > 0)
            std::printf("  Avg poll read:      %ld ns\n", totalPollNs / pollCount);
        if (eventsFound > 0)
            std::printf("  Avg event parse:    %ld ns (2x 64-bit reads)\n",
                        totalParseNs / eventsFound);
        std::printf("\n");
    }

private:
    // Read a single 32-bit word from the async ring buffer at the given word index
    [[nodiscard]]
    std::uint32_t readRingWord(std::uint32_t wordIndex) const {
        return *m_pcieHandler.registerPtr<std::uint32_t>(WREN_ASYNC_DATA_BASE + wordIndex * 4);
    }

    PCIeBackend m_pcieHandler;
    bool m_connected{};
};

#endif //FPGA_DRIVER_WRENTESTER_HPP
