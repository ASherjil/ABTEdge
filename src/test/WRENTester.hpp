//
// Created by asherjil on 2/14/26.
//

#ifndef FPGA_DRIVER_WRENTESTER_HPP
#define FPGA_DRIVER_WRENTESTER_HPP
#include "backends/PCIeBackend.hpp"
#include <cstdint>
#include <cstdio>

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

// ── Mailbox register offsets (from BAR1 base) ──
// mb_map.h: struct is at BAR1+0x10000
constexpr std::uint32_t B2H_CSR  = 0x10000;          // poll this for reply
constexpr std::uint32_t B2H_CMD  = 0x10008;          // reply command
constexpr std::uint32_t B2H_LEN  = 0x1000C;          // reply length (words)
constexpr std::uint32_t B2H_DATA = 0x10010;          // reply data

constexpr std::uint32_t H2B_CSR  = 0x11000;          // write READY to send
constexpr std::uint32_t H2B_CMD  = 0x11008;          // command ID
constexpr std::uint32_t H2B_LEN  = 0x1100C;          // data length (words)
constexpr std::uint32_t H2B_DATA = 0x11010;          // data words

constexpr std::uint32_t MB_CSR_READY = 0x1;
constexpr std::uint32_t CMD_REPLY    = 0x80000000;
constexpr std::uint32_t CMD_ERROR    = 0x40000000;

// ── Command IDs (counted from wren-mb-cmds.def) ──
constexpr std::uint32_t CMD_RX_SET_COND   = 18;
constexpr std::uint32_t CMD_RX_SET_ACTION = 21;
constexpr std::uint32_t CMD_RX_SUBSCRIBE  = 38;

// ── Async ring (same offsets you already use in ABTWREN) ──
constexpr std::uint32_t ASYNC_DATA  = 0x12000;
constexpr std::uint32_t ASYNC_BOARD = 0x14000;

// ── Capsule types ──
constexpr std::uint8_t ASYNC_EVENT  = 0x02;
constexpr std::uint8_t ASYNC_CONFIG = 0x03;
constexpr std::uint8_t ASYNC_PULSE  = 0x04;

// ── Pulser flags (from wren-mb-defs.h:162-168) ──
constexpr std::uint8_t FLAG_INT_EN = (1 << 2);       // 0x04 — generate interrupt
constexpr std::uint8_t FLAG_ENABLE = (1 << 7);       // 0x80 — enable config

// ── Our chosen indices (high range, won't collide with LTIM driver) ──
constexpr std::uint16_t OUR_COND_IDX = 1100;
constexpr std::uint32_t OUR_ACT_IDX  = 2040;
constexpr std::uint8_t  OUR_PULSER   = 29;           // channel 30, free
constexpr std::uint32_t OUR_SRC_IDX  = 0;            // primary WR source
constexpr std::uint32_t OUR_EV_ID    = 142;          // PIX.AMCLO-CT

constexpr std::uint16_t INPUT_NOSTART   = 31;   // AUTO — fire on comparator match, no ext trigger
constexpr std::uint16_t INPUT_NOSTOP    = 31;   // NONE
constexpr std::uint16_t INPUT_CLK_1KHZ  = 23;
constexpr std::uint16_t INPUT_CLK_10MHZ = 25;
constexpr std::uint16_t INPUT_CLK_1GHZ  = 31;

constexpr std::uint32_t CMD_RX_DEL_ACTION = 22;
constexpr std::uint32_t CMD_RX_DEL_COND   = 19;

class WRENTester {
public:
    WRENTester();

    ~WRENTester();

    // Bulk read all safe host registers with timing
    // Uses 64-bit reads to pair adjacent registers (7 PCIe round-trips instead of 12)
    void performHostRegisterDump();

    // Busy-poll the async ring buffer for CTIM/LTIM events with due times
    // Self-terminates after durationSeconds as a watchdog safety measure
    void pollTimingEvents(int durationSeconds = 30);

    // Subscribe to CTIM via mailbox, then poll ring buffer for EVENT/CONFIG/PULSE capsules.
    // Dynamically learns sw_cmp_idx from CONFIG to match our pulser's PULSE capsules.
    void trackCTIMEvents(int durationSeconds = 30);

    void setupCTIMSubscription();
private:
    // Read a single 32-bit word from the async ring buffer at the given word index
    [[nodiscard, gnu::always_inline]]
    inline std::uint32_t readRingWord(std::uint32_t wordIndex) const {
        return *m_pcieHandler.registerPtr<std::uint32_t>(WREN_ASYNC_DATA_BASE + wordIndex * 4);
    }

    bool mbSend(std::uint32_t cmd, const void* data, std::size_t words) ;
    void cleanupCTIMSubscription();

    PCIeBackend m_pcieHandler;
    bool m_connected{};
};

#endif //FPGA_DRIVER_WRENTESTER_HPP
