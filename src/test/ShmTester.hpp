// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Areeb Sherjil
//
// Created by asherjil on 4/4/26.
//
// ShmTester — Validates ShmBackend by writing from a Writer and reading from a Reader
//             within the same process via two separate mmap'd views of /dev/shm.
//
// Usage:
//   ShmTester tester;
//   tester.runWriteReadTest();
//

#ifndef FPGA_DRIVER_SHMTESTER_HPP
#define FPGA_DRIVER_SHMTESTER_HPP

#include "backends/ShmBackend.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

// ── Example shared memory layout ──
// User defines their own payload — must be trivially copyable
struct TestPayload {
    std::uint32_t eventId;
    std::uint32_t sec;
    std::uint32_t nsec;
    std::uint8_t  channel;
    std::uint8_t  padding[3]; // explicit padding for clarity
};
static_assert(std::is_trivially_copyable_v<TestPayload>);
static_assert(sizeof(TestPayload) == 16);

// Cache-line-aligned slot with atomic sequence counter for seqlock protocol
struct alignas(CACHE_LINE_SIZE) TestSlot {
    std::atomic<std::uint64_t> seq;
    TestPayload                payload;
};
static_assert(sizeof(TestSlot) >= CACHE_LINE_SIZE);
static_assert(alignof(TestSlot) == CACHE_LINE_SIZE);

constexpr const char* TEST_SHM_NAME = "/abtedge_shm_test";

class ShmTester {
public:
    void runWriteReadTest() {
        std::printf("\n=== ShmBackend Write/Read Test ===\n");

        // ── Step 1: Writer creates shared memory and initializes the atomic ──
        ShmBackend<ShmMode::Writer> writer;
        if (!writer.open<TestSlot>(TEST_SHM_NAME, &TestSlot::seq)) {
            std::printf("  FAIL: Writer failed to open shared memory\n");
            return;
        }
        std::printf("  Writer opened: base=%p  size=%zu\n",
                    writer.getBaseAddress(), writer.getMmapSize());

        // ── Step 2: Write a test event with release semantics ──
        auto* wSlot = writer.ptr<TestSlot>();

        TestPayload testEvent{};
        testEvent.eventId = 42;
        testEvent.sec     = 1712345678;
        testEvent.nsec    = 500000000;
        testEvent.channel = 7;

        wSlot->payload = testEvent;
        wSlot->seq.store(1, std::memory_order_release);

        std::printf("  Writer published: eventId=%u  sec=%u  nsec=%u  channel=%u  seq=1\n",
                    testEvent.eventId, testEvent.sec, testEvent.nsec, testEvent.channel);

        // ── Step 3: Reader opens the same shared memory region ──
        ShmBackend<ShmMode::Reader> reader;
        if (!reader.open<TestSlot>(TEST_SHM_NAME)) {
            std::printf("  FAIL: Reader failed to open shared memory\n");
            return;
        }
        std::printf("  Reader opened: base=%p  size=%zu\n",
                    reader.getBaseAddress(), reader.getMmapSize());

        // ── Step 4: Seqlock read with torn-read guard ──
        auto* rSlot = reader.ptr<TestSlot>();

        std::uint64_t s1 = rSlot->seq.load(std::memory_order_acquire);
        TestPayload readBack{};
        std::memcpy(&readBack, &rSlot->payload, sizeof(TestPayload));
        std::uint64_t s2 = rSlot->seq.load(std::memory_order_acquire);

        std::printf("  Reader received: eventId=%u  sec=%u  nsec=%u  channel=%u  seq1=%lu  seq2=%lu\n",
                    readBack.eventId, readBack.sec, readBack.nsec, readBack.channel, s1, s2);

        // ── Step 5: Verify ──
        bool seqValid   = (s1 == s2) && (s1 == 1);
        bool dataValid  = (readBack.eventId == 42)
                       && (readBack.sec     == 1712345678)
                       && (readBack.nsec    == 500000000)
                       && (readBack.channel == 7);

        std::printf("\n  Sequence check: %s\n", seqValid  ? "PASS" : "FAIL");
        std::printf("  Data check:     %s\n",   dataValid ? "PASS" : "FAIL");
        std::printf("  Overall:        %s\n\n", (seqValid && dataValid) ? "PASS" : "FAIL");

        // ── Step 6: Writer closes first (unlinks /dev/shm), reader closes after ──
        // Demonstrates correct ownership: writer owns the shm lifetime
        writer.close();
        reader.close();
    }
};

#endif //FPGA_DRIVER_SHMTESTER_HPP
