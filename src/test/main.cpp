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

#ifdef FPGA_PLATFORM_X86_PCIE
    x86_64Tuner tuner(4, 99);  // pin to core 4, SCHED_FIFO:99
#else
    // ARM64 fallback: basic RT setup only
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
        std::fprintf(stderr, "WARNING: mlockall failed (run as root)\n");
    struct sched_param sp{.sched_priority = 99};
    if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0)
        std::fprintf(stderr, "WARNING: SCHED_FIFO failed (run as root)\n");
#endif

#ifdef FPGA_PLATFORM_ARM_AXI
    AXIBusTester diotTester;
    diotTester.performReadWriteTest();
    diotTester.generateSquarePulse();
#endif

#ifdef FPGA_PLATFORM_X86_PCIE
/*
    TXMC635Tester txmc635Tester;
    txmc635Tester.performCPLDLatencyTest();
    txmc635Tester.performADCAcquisition();
*/
    WRENTester wrenTester;
    wrenTester.performHostRegisterDump();
    //wrenTester.pollTimingEvents(30);
    wrenTester.measureLtimPulseJitter(30);
#endif

    return 0;
}