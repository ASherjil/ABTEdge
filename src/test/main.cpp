//
// Created by asherjil on 2/5/26.
//

#include "dummyRegisters.hpp"
#include "TXMC635Tester.hpp"
#include "WRENTester.hpp"
#include "x86_64Tuner.hpp"
#include <cstdio>
#include <chrono>
#include <thread>
#include <sched.h>      // sched_setscheduler, SCHED_FIFO
#include <sys/mman.h>   // mlockall

using namespace fpga::regs::dummy_hw_desc;
using namespace std::chrono_literals;

int main() {
    // Lock all current and future pages in RAM — prevents page faults
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        std::fprintf(stderr, "WARNING: mlockall failed (run as root)\n");
    }

    // Set real-time scheduling — prevents kernel preemption during measurements
    struct sched_param sp{.sched_priority = 99};
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) {
        std::fprintf(stderr, "WARNING: SCHED_FIFO failed (run as root)\n");
    }

    // Optional: full system tuning (IRQ isolation, CPU pinning, governor)
    // #ifdef FPGA_PLATFORM_X86_PCIE
    //     x86_64Tuner tuner(4, 99);
    // #endif

#ifdef FPGA_PLATFORM_ARM_AXI
    AXIBusTester diotTester;
    diotTester.performReadWriteTest();
    diotTester.generateSquarePulse();
#endif

#ifdef FPGA_PLATFORM_X86_PCIE

    TXMC635Tester txmc635Tester;
    txmc635Tester.performCPLDLatencyTest();
    txmc635Tester.performADCAcquisition();

    WRENTester wrenTester;
    wrenTester.performHostRegisterDump();
    //wrenTester.pollTimingEvents(30);
    wrenTester.trackCTIMEvents(30);

#endif

    return 0;
}