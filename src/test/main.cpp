//
// Created by asherjil on 2/5/26.
//

#include "dummyRegisters.hpp"
#include <cstdio>
#include <chrono>
#include <thread>

using namespace fpga::regs::dummy_hw_desc;
using namespace std::chrono_literals;

int main() {
    AXIBusTester diotTester;
    diotTester.performReadWriteTest();
    diotTester.generateSquarePulse();

    return 0;
}