// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Areeb Sherjil
//
// seqlock_test — runs SeqLockTester without any FPGA hardware.
//
// Usage: seqlock_test [publishes] [readers]      (default 10'000'000, 3)
// Run as root for mlockall and SCHED_FIFO, as latency_test does.

#include "SeqLockTester.hpp"

#include <cstdio>
#include <cstdlib>
#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    std::uint64_t publishes = 10'000'000;
    unsigned      readers   = 3;
    if (argc > 1) {
        publishes = std::strtoull(argv[1], nullptr, 10);
    }
    if (argc > 2) {
        readers = static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10));
    }

    // Only as root: as a normal user mlockall(MCL_FUTURE) locks every new thread
    // stack and thread creation fails on the memlock limit.
    if (::geteuid() == 0) {
        if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
            std::fprintf(stderr, "WARNING: mlockall failed\n");
        }
        struct sched_param sp{.sched_priority = 99};
        if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) {
            std::fprintf(stderr, "WARNING: SCHED_FIFO failed\n");
        }
    }
    else {
        std::fprintf(stderr, "NOTE: not root, running without mlockall and SCHED_FIFO\n");
    }

    SeqLockTester tester;
    return tester.runAll(publishes, readers) ? 0 : 1;
}
