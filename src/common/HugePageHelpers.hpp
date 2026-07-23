// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Areeb Sherjil
//
// Created by asherjil on 4/4/26.
//

#ifndef ABTEDGE_HUGEPAGEHELPERS_HPP
#define ABTEDGE_HUGEPAGEHELPERS_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <string_view>
#include <unistd.h>

inline constexpr std::string_view SYSFS_HUGEPAGE_NR   = "/sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages";
inline constexpr std::string_view SYSFS_HUGEPAGE_FREE = "/sys/kernel/mm/hugepages/hugepages-2048kB/free_hugepages";

[[nodiscard]] inline std::uint64_t resolvePhysicalAddress(const void* virtualAddress){
    int fd = ::open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0){
        std::fprintf(stderr, "[Error] Unable to open /proc/self/pagemap\n");
        return 0;
    }

    std::uintptr_t vaddr    = reinterpret_cast<std::uintptr_t>(virtualAddress);
    std::size_t pageSize    = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE)); // 4096 on x86-64
    std::uint64_t pageIndex = vaddr / pageSize;

    std::uint64_t entry{};
    ssize_t bytesRead =::pread(fd, &entry, sizeof(entry), static_cast<off_t>(pageIndex * sizeof(entry)));

    ::close(fd);

    if (bytesRead != sizeof(entry)) {
        return 0;
    }

    // Bit 63 : page present - if not set, page is swapped or unmapped
    if (!(entry & (1ULL << 63))) {
        return 0;
    }

    // Bits [0:54] : page frame number
    std::uint64_t pfn    = entry & ((1ULL << 55) - 1);
    std::uint64_t offset = vaddr % pageSize;

    return pfn * pageSize + offset;
}


/// Reads a single integer from a sysfs file. Returns -1 on failure.
[[nodiscard]] inline int readSysfsInt(std::string_view path) {
   std::ifstream ifs{std::string(path)};
   int value{-1};
   ifs >> value;
   return value;
}

/// Ensures at least 'required' free 2MB hugepages are available.
/// Reads current state, computes the deficit, and requests only what is needed.
/// Requires root. Returns true if 'required' free hugepages are confirmed available.
[[nodiscard]] inline bool ensureHugepages(int required) {
   if (required <= 0) {
       return false;
   }

   int free = readSysfsInt(SYSFS_HUGEPAGE_FREE);
   if (free >= required) {
       return true;
   }

   if (free < 0) {
       std::fprintf(stderr, "[Error] Cannot read %s\n", std::string(SYSFS_HUGEPAGE_FREE).c_str());
       return false;
   }

   int total = readSysfsInt(SYSFS_HUGEPAGE_NR);
   if (total < 0) {
       std::fprintf(stderr, "[Error] Cannot read %s\n", std::string(SYSFS_HUGEPAGE_NR).c_str());
       return false;
   }

   {
     int needed = total + (required - free);
     std::ofstream ofs{std::string(SYSFS_HUGEPAGE_NR)};
       if (!ofs) {
           std::fprintf(stderr, "[Error] Cannot write to %s (requires root)\n",
                        std::string(SYSFS_HUGEPAGE_NR).c_str());
           return false;
       }
       ofs << needed;
       if (!ofs.good()) {
           std::fprintf(stderr, "[Error] Write failed to %s\n",
                        std::string(SYSFS_HUGEPAGE_NR).c_str());
           return false;
       }
   } // ofs closed via RAII

   // Verify the kernel honoured the request — allocation can silently fail
   // if physical memory is too fragmented for contiguous 2MB pages
   if (int verified = readSysfsInt(SYSFS_HUGEPAGE_FREE); verified < required) {
       std::fprintf(stderr, "[Error] Hugepage allocation incomplete: requested %d, got %d free "
                    "(memory may be too fragmented)\n", required, verified);
       return false;
   }

   return true;
}


#endif //ABTEDGE_HUGEPAGEHELPERS_HPP
