#ifndef FPGA_DRIVER_X86_64TUNER_HPP
#define FPGA_DRIVER_X86_64TUNER_HPP

#ifdef ABTEDGE_ARCH_X86_64

// Applies low-latency system tuning for PCIe busy-poll workloads on x86_64.
// All settings persist until reboot — destructor intentionally does not undo.
class x86_64Tuner {
public:
    // Applies all tuning immediately. Requires root.
    x86_64Tuner(int cpuCore, int schedPriority);
    ~x86_64Tuner() = default;

    x86_64Tuner(const x86_64Tuner&) = delete;
    x86_64Tuner& operator=(const x86_64Tuner&) = delete;
    x86_64Tuner(x86_64Tuner&&) = delete;
    x86_64Tuner& operator=(x86_64Tuner&&) = delete;
};

#endif // ABTEDGE_ARCH_X86_64
#endif // FPGA_DRIVER_X86_64TUNER_HPP
