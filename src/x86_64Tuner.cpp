#include "x86_64Tuner.hpp"

#ifdef FPGA_PLATFORM_X86_PCIE

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <dirent.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>

// =============================================================================
// Helpers (file-local)
// =============================================================================

namespace {

bool writeFile(const char* path, const char* value) {
    int fd = ::open(path, O_WRONLY | O_TRUNC);
    if (fd < 0) return false;
    ssize_t n = ::write(fd, value, std::strlen(value));
    ::close(fd);
    return n > 0;
}

std::string readFile(const char* path) {
    int fd = ::open(path, O_RDONLY);
    if (fd < 0) return {};
    char buf[256];
    ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
    ::close(fd);
    if (n <= 0) return {};
    buf[n] = '\0';
    if (buf[n - 1] == '\n') buf[n - 1] = '\0';
    return buf;
}

std::string cpuListExcluding(int core) {
    long nproc = ::sysconf(_SC_NPROCESSORS_ONLN);
    std::string result;
    for (int i = 0; i < nproc; ++i) {
        if (i == core) continue;
        if (!result.empty()) result += ',';
        result += std::to_string(i);
    }
    return result;
}

} // namespace

// =============================================================================
// Constructor — apply all tuning (requires root)
// =============================================================================

x86_64Tuner::x86_64Tuner(int cpuCore, int schedPriority)
{
    std::fprintf(stderr, "[x86_64Tuner] Applying low-latency tuning on core %d...\n", cpuCore);
    char path[128];

    // --- 1. Stop irqbalance (prevents undoing our IRQ pinning) ---
    if (std::system("systemctl stop irqbalance 2>/dev/null") == 0)
        std::fprintf(stderr, "[x86_64Tuner] irqbalance stopped\n");

    // --- 2. Disable RT throttling (prevents 50ms forced sleep every 1s) ---
    auto oldRt = readFile("/proc/sys/kernel/sched_rt_runtime_us");
    if (writeFile("/proc/sys/kernel/sched_rt_runtime_us", "-1"))
        std::fprintf(stderr, "[x86_64Tuner] sched_rt_runtime_us: %s -> -1\n",
                     oldRt.empty() ? "?" : oldRt.c_str());
    else
        std::fprintf(stderr, "[x86_64Tuner] WARNING: could not disable RT throttling\n");

    // --- 3. CPU governor -> "performance" (prevents frequency scaling) ---
    std::snprintf(path, sizeof(path),
                  "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", cpuCore);
    auto oldGov = readFile(path);
    if (!oldGov.empty()) {
        if (writeFile(path, "performance"))
            std::fprintf(stderr, "[x86_64Tuner] cpu%d governor: %s -> performance\n",
                         cpuCore, oldGov.c_str());
        else
            std::fprintf(stderr, "[x86_64Tuner] WARNING: could not set governor for cpu%d\n",
                         cpuCore);
    } else {
        std::fprintf(stderr, "[x86_64Tuner] cpu%d cpufreq not available (skipped)\n", cpuCore);
    }

    // --- 4. Move ALL IRQs off target core ---
    std::string mask = cpuListExcluding(cpuCore);
    int moved = 0;
    DIR* irqDir = ::opendir("/proc/irq");
    if (irqDir) {
        while (auto* entry = ::readdir(irqDir)) {
            if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;
            int irq = std::atoi(entry->d_name);
            if (irq == 0) continue;
            std::snprintf(path, sizeof(path), "/proc/irq/%d/smp_affinity_list", irq);
            if (writeFile(path, mask.c_str())) moved++;
        }
        ::closedir(irqDir);
    }
    std::fprintf(stderr, "[x86_64Tuner] Moved %d IRQs off core %d\n", moved, cpuCore);

    // --- 5. Pin process to target core ---
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpuCore, &cpuset);
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == 0)
        std::fprintf(stderr, "[x86_64Tuner] Process pinned to core %d\n", cpuCore);
    else
        std::fprintf(stderr, "[x86_64Tuner] WARNING: sched_setaffinity failed: %s\n",
                     std::strerror(errno));

    // --- 6. SCHED_FIFO at given priority ---
    sched_param sp{};
    sp.sched_priority = schedPriority;
    if (sched_setscheduler(0, SCHED_FIFO, &sp) == 0)
        std::fprintf(stderr, "[x86_64Tuner] SCHED_FIFO:%d set\n", schedPriority);
    else
        std::fprintf(stderr, "[x86_64Tuner] WARNING: SCHED_FIFO:%d failed: %s\n",
                     schedPriority, std::strerror(errno));

    // --- 7. Lock all memory (prevents page faults) ---
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == 0)
        std::fprintf(stderr, "[x86_64Tuner] mlockall OK\n");
    else
        std::fprintf(stderr, "[x86_64Tuner] WARNING: mlockall failed: %s\n",
                     std::strerror(errno));

    std::fprintf(stderr, "[x86_64Tuner] All tuning applied.\n");
}

#endif // FPGA_PLATFORM_X86_PCIE
