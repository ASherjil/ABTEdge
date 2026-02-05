//
// Created by asherjil on 2/5/26.
//

#include "backends/AXIBackend.hpp"
#include "dummyRegisters.hpp"
#include <cstdio>
#include <chrono>
#include <thread>


using namespace fpga::regs::dummy_hw_desc;
using namespace std::chrono_literals;

int main() {
    // found by running "ls /sys/bus/platform/devices/ | grep -i dummy" on the DIOT
    constexpr std::size_t dummyPhysicalAddress = 0xB0020000; // found from the device tree

    AXIBackend axiHandler;

    if (!axiHandler.open("/dev/mem", dummyPhysicalAddress, 0x1000 /*4KB is enough*/)) {
        std::perror("Failed to open /dev/mem");
        return 1;
    }

    while (true) {
        // Generate a square pulse every 1 second
        *axiHandler.registerPtr<std::uint32_t>(MEAS_JITTER) = 1;
        *axiHandler.registerPtr<std::uint32_t>(MEAS_JITTER) = 0;

        std::this_thread::sleep_for(1000ms);
    }
}