// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Areeb Sherjil
//
// Created by asherjil on 10/22/26.
//

#ifndef ABTEDGE_CACHELINE_H
#define ABTEDGE_CACHELINE_H

#include <cstddef>
#include <new>

// 64 on x86-64. GCC reports 256 on aarch64 (see the ShmBackend aarch64 fix).
#ifdef __cpp_lib_hardware_interference_size
constexpr std::size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
#else
constexpr std::size_t CACHE_LINE_SIZE = 64;
#endif

#endif // ABTEDGE_CACHELINE_H
