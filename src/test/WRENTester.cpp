// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Areeb Sherjil
//
// Created by asherjil on 2/20/26.
//

#include "WRENTester.hpp"
#include <chrono>

WRENTester::WRENTester()
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

WRENTester::~WRENTester() {
    cleanupCTIMSubscription(); // delete the created LTIM
}

void WRENTester::performHostRegisterDump() {
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

void WRENTester::pollTimingEvents(int durationSeconds) {
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

void WRENTester::trackCTIMEvents(int durationSeconds) {
    if (!m_connected) {
        std::printf("WREN connection failed, skipping test\n");
        return;
    }

    // Setup: subscribe to our CTIM and configure pulser via mailbox
    setupCTIMSubscription();

    constexpr std::uint32_t RING_MASK = 2047;
    constexpr std::uint32_t TYP_MASK  = 0x000000FF;
    constexpr unsigned      LEN_SHIFT = 16;

    std::printf("\n=== CTIM Tracker (ev_id=%u, pulser=%u, watchdog=%ds) ===\n\n",
                OUR_EV_ID, OUR_PULSER, durationSeconds);

    // Sync to current ring position
    std::uint32_t shadowOff = *m_pcieHandler.registerPtr<std::uint32_t>(WREN_ASYNC_BOARD_OFF);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(durationSeconds);

    // CONFIG tracking: firmware reuses sw_cmp from a pool, and there's a ~3.4s gap
    // between CONFIG (comparator loaded) and PULSE (comparator fires).  Multiple CONFIGs
    // can be pending simultaneously, so track ALL of them — not just the latest.
    bool ourComps[512] = {};  // ourComps[sw_cmp] = true means it belongs to our action

    // Stats
    int eventCount = 0, pulseCount = 0, configCount = 0;

    while (std::chrono::steady_clock::now() < deadline) {
        std::uint32_t boardOff = *m_pcieHandler.registerPtr<std::uint32_t>(WREN_ASYNC_BOARD_OFF);
        if (boardOff == shadowOff)
            continue;

        do {
            bool safe64 = (shadowOff + 3) <= RING_MASK;

            std::uint32_t hdr, w1;
            if (safe64) [[likely]] {
                // 64-bit read: header (w0) + w1 in one PCIe transaction
                std::uint64_t hdrW1 = *m_pcieHandler.registerPtr<std::uint64_t>(
                    WREN_ASYNC_DATA_BASE + shadowOff * 4);
                hdr = static_cast<std::uint32_t>(hdrW1);
                w1  = static_cast<std::uint32_t>(hdrW1 >> 32);
            } else {
                // Wrap boundary fallback (once per full ring cycle)
                hdr = readRingWord(shadowOff);
                w1  = readRingWord((shadowOff + 1) & RING_MASK);
            }

            auto typ = static_cast<std::uint8_t>(hdr & TYP_MASK);
            auto len = static_cast<std::uint16_t>(hdr >> LEN_SHIFT);

            if (len == 0 || len > 2048) {
                std::printf("  [ERROR] Invalid capsule len=%u at offset=%u\n", len, shadowOff);
                goto done;
            }

            switch (typ) {

            case ASYNC_PULSE: {
                auto swCmp = static_cast<std::uint16_t>(w1);
                if (swCmp >= 512 || !ourComps[swCmp])
                    break;
                ourComps[swCmp] = false;  // consumed — comparator is done

                // 64-bit read: pulse timestamp (w2=sec, w3=nsec)
                std::uint32_t sec, nsec;
                if (safe64) [[likely]] {
                    std::uint64_t w2w3 = *m_pcieHandler.registerPtr<std::uint64_t>(
                        WREN_ASYNC_DATA_BASE + (shadowOff + 2) * 4);
                    sec  = static_cast<std::uint32_t>(w2w3);
                    nsec = static_cast<std::uint32_t>(w2w3 >> 32);
                } else {
                    sec  = readRingWord((shadowOff + 2) & RING_MASK);
                    nsec = readRingWord((shadowOff + 3) & RING_MASK);
                }

                ++pulseCount;
                std::printf("  PULSE #%-3d  pulser=%u  sw_cmp=%u  fired=%u.%09u\n",
                            pulseCount, OUR_PULSER, swCmp, sec, nsec);
                break;
            }

            case ASYNC_CONFIG: {
                // w1 = {sw_cmp_idx[31:16], act_idx[15:0]}
                auto actIdx = static_cast<std::uint16_t>(w1);
                auto swCmp  = static_cast<std::uint16_t>(w1 >> 16);
                if (actIdx == OUR_ACT_IDX && swCmp < 512) {
                    ourComps[swCmp] = true;
                    std::printf("  CONFIG  act=%u -> sw_cmp=%u  pulser=%u (ours)\n", actIdx, swCmp, OUR_PULSER);
                }
                ++configCount;
                break;
            }

            case ASYNC_EVENT: {
                auto evId = static_cast<std::uint16_t>(w1 & 0xFFFF);
                if (evId != OUR_EV_ID) break;

                // 64-bit read: event timestamp (w2=nsec, w3=sec — opposite of pulse)
                std::uint32_t nsec, sec;
                if (safe64) [[likely]] {
                    std::uint64_t w2w3 = *m_pcieHandler.registerPtr<std::uint64_t>(
                        WREN_ASYNC_DATA_BASE + (shadowOff + 2) * 4);
                    nsec = static_cast<std::uint32_t>(w2w3);
                    sec  = static_cast<std::uint32_t>(w2w3 >> 32);
                } else {
                    nsec = readRingWord((shadowOff + 2) & RING_MASK);
                    sec  = readRingWord((shadowOff + 3) & RING_MASK);
                }

                ++eventCount;

                std::printf("  EVENT #%-3d  ev_id=%u  due=%u.%09u\n", eventCount, evId, sec, nsec);
                break;
            }

            default:
                break;
            }

            shadowOff = (shadowOff + len) & RING_MASK;
        } while (shadowOff != boardOff);
    }

done:
    std::printf("\n  --- Summary ---\n");
    std::printf("  Events (CTIMs):   %d\n", eventCount);
    std::printf("  Configs:          %d\n", configCount);
    std::printf("  Pulses:           %d\n", pulseCount);
    std::printf("\n");
}


void WRENTester::setupCTIMSubscription() {
    if (!m_connected) {
        std::printf("WREN connection failed, cannot setup CTIM subscription\n");
        return;
    }
    // 1. Subscribe to our event ID/CTIM
    // struct wren_mb_rx_subscribe { uint32_t src_idx; uint32_t ev_id; }
    struct {
        std::uint32_t src_idx;
        std::uint32_t ev_id;
    } sub = { OUR_SRC_IDX, OUR_EV_ID };

    std::printf("SUBSCRIBE src=%u ev_id=%u ...\n", sub.src_idx, sub.ev_id);

    if (!mbSend(CMD_RX_SUBSCRIBE, &sub, sizeof(sub) / 4)) {
        std::printf("RX_SUBSCRIBE command send failure returning.");
        return;
    }

    // 2. Set condition: match event ID on source 0
    // struct wren_mb_rx_set_cond:
    //   uint16_t cond_idx
    //   struct wren_mb_cond { uint16_t evt_id, uint8_t src_idx, uint8_t len, uint32_t ops[8] }
    //
    // NOTE: wren_mb_cond has 4-byte alignment (contains uint32_t ops[]),
    //       so there are 2 bytes of padding after cond_idx.
    //       Safest approach: build word-by-word.

    std::uint32_t data[10] = {};
    // Word 0: [cond_idx:16][padding:16]
    data[0] = OUR_COND_IDX;            // low 16 bits = cond_idx, high 16 = pad (0)
    // Word 1: [evt_id:16][src_idx:8][len:8]
    data[1] = OUR_EV_ID                // low 16 bits = evt_id
            | (OUR_SRC_IDX << 16)      // byte 2 of this word = src_idx
            | (0u << 24);              // byte 3 = len (0 = unconditional, no ops)
    // Words 2-9: ops[0..7] = all zeros (unused since len=0)

    std::printf("SET_COND cond_idx=%u evt_id=%u src=%u ...\n",
                OUR_COND_IDX, OUR_EV_ID, OUR_SRC_IDX);

    if (!mbSend(CMD_RX_SET_COND, data, 10)) {
        std::printf("RX_SET_COND command send failure returning.");
        return;
    }

    // 3. Set action at pulser 29, CTIM event time + 0 ms offset
    // struct wren_mb_rx_set_action:
    //   uint32_t act_idx
    //   uint32_t cond_idx
    //   struct wren_mb_pulser_config { ... }
    //
    // No padding issues here — all fields naturally aligned.

    struct {
        std::uint32_t act_idx;
        std::uint32_t cond_idx;
        // wren_mb_pulser_config:
        std::uint8_t  pulser_idx;
        std::uint8_t  flags;
        std::uint16_t inputs;           // start[0:4]|stop[5:9]|clock[10:14]
        std::uint32_t width;
        std::uint32_t period;
        std::uint32_t npulses;
        std::uint32_t idelay;
        std::int32_t  load_off_sec;
        std::int32_t  load_off_nsec;
    } act = {};

    act.act_idx    = OUR_ACT_IDX;
    act.cond_idx   = OUR_COND_IDX;
    act.pulser_idx = OUR_PULSER;
    act.flags      = FLAG_ENABLE | FLAG_INT_EN;   // 0x84
    act.inputs     = (INPUT_CLK_1KHZ << 10) | (INPUT_NOSTOP << 5) | INPUT_NOSTART;
    act.width      = 1000;                        // 1us pulse (just need the interrupt)
    act.period     = 1;
    act.npulses    = 1;
    act.idelay     = 0;
    act.load_off_sec  = 0;                        // ZERO offset — fire at CTIM due time
    act.load_off_nsec = 0;

    std::printf("SET_ACTION act=%u cond=%u pulser=%u flags=0x%02X offset=0 ...\n",
                act.act_idx, act.cond_idx, act.pulser_idx, act.flags);

    if (!mbSend(CMD_RX_SET_ACTION, &act, sizeof(act) / 4)) {
        std::printf("RX_SET_ACTION command send failure returning.");
    }
}


bool WRENTester::mbSend(std::uint32_t cmd, const void* data, std::size_t words) {
    const std::uint32_t* src = static_cast<const std::uint32_t*>(data);

    // 1: Write data words to H2B_DATA
    for (std::size_t i{}; i<words; i++) {
        *m_pcieHandler.registerPtr<std::uint32_t>(H2B_DATA + i * 4) = src[i];
    }

    // 2: Write command ID and length
    *m_pcieHandler.registerPtr<std::uint32_t>(H2B_CMD) = cmd;
    *m_pcieHandler.registerPtr<std::uint32_t>(H2B_LEN) = static_cast<std::uint32_t>(words);

    // 3: Fire - firmware starts processing
    *m_pcieHandler.registerPtr<std::uint32_t>(H2B_CSR) = MB_CSR_READY;

    // 4: Poll the B2H_CSR for reply, timeout after ~1s
    for (int i{}; i<1'000'000; i++) {
        if (*m_pcieHandler.registerPtr<std::uint32_t>(B2H_CSR) & MB_CSR_READY) {
            goto got_reply;
        }
    }

    std::fprintf(stderr, "  TIMEOUT waiting for firmware reply\n");
    return false;

    got_reply:

    // 5: Check the reply
    std::uint32_t reply =  *m_pcieHandler.registerPtr<std::uint32_t>(B2H_CMD);

    // 6: Acknowledge (clear B2H_CSR)
    *m_pcieHandler.registerPtr<std::uint32_t>(B2H_CSR) = 0;

    if (reply & CMD_ERROR) {
        std::fprintf(stderr, "  FIRMWARE REJECTED cmd %u (reply=0x%08X)\n", cmd, reply);
        return false;
    }

    std::printf("  OK (reply=0x%08X)\n", reply);
    return true;
}

void WRENTester::cleanupCTIMSubscription()
{
    // 1. Delete action FIRST (unlinks from condition)
    const std::uint32_t actIdx = OUR_ACT_IDX;   // 2040
    if (!mbSend(CMD_RX_DEL_ACTION, &actIdx, 1)) {
        std::printf("DEL_ACTION act=%u failed!\n", actIdx);
    }
    else {
        std::printf("DEL_ACTION act=%u completed\n", actIdx);
    }

    // 2. Now delete condition (safe — no action linked)
    const std::uint32_t condIdx = OUR_COND_IDX; // 1100
    if (!mbSend(CMD_RX_DEL_COND, &condIdx, 1)) {
        std::printf("DEL_COND cond=%u failed!\n", condIdx);
    }
    else {
        std::printf("DEL_COND cond=%u completed\n", condIdx);
    }
    // Do NOT unsubscribe ev142 — channel 23's LTIM also uses it
}
