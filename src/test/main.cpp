//
// Created by asherjil on 2/5/26.
//

#include "dummyRegisters.hpp"
#include "TXMC635Tester.hpp"
#include <cstdio>
#include <chrono>
#include <thread>

using namespace fpga::regs::dummy_hw_desc;
using namespace std::chrono_literals;

int main() {
#ifdef FPGA_PLATFORM_ARM_AXI
    AXIBusTester diotTester;
    diotTester.performReadWriteTest();
    diotTester.generateSquarePulse();
#endif

#ifdef FPGA_PLATFORM_X86_PCIE
    TXMC635Tester txmc635Tester;
    txmc635Tester.performCPLDLatencyTest();
    txmc635Tester.performADCAcquisition();
#endif

    return 0;
}