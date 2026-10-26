// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Areeb Sherjil
//
// SeqLockTester — tests T1..T5 of docs/SEQLOCK_REQUIREMENTS.md.
//
//   T1  single-threaded round trip
//   T2  torn-read detection: one writer, N pinned readers, invariant on every load
//   T3  sequence semantics: even, monotonic, missed-publish detection, try_load
//   T4  shared-memory placement: writer and reader views through two ShmBackend mappings
//   T5  payload size sweep (16 B .. 4 KiB), folded into T1/T2
//
// Usage:
//   SeqLockTester tester;
//   bool ok = tester.runAll(publishes, readers);

#ifndef ABTEDGE_SEQLOCKTESTER_HPP
#define ABTEDGE_SEQLOCKTESTER_HPP

#include "common/SeqLock.hpp"
#include "backends/ShmBackend.hpp"

#include <atomic>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <pthread.h>
#include <sched.h>
#include <thread>
#include <vector>

// ── Payloads: every byte derived from one counter so a torn copy is detectable ──
template <std::size_t N>
struct SeqLockTestPayload {
    std::uint64_t a;
    std::uint64_t b;              // ~a
    std::uint8_t  filler[N - 16]; // filler[k] == uint8_t(a + k)
};

template <>
struct SeqLockTestPayload<16> {
    std::uint64_t a;
    std::uint64_t b;
};

template <std::size_t N>
void fillPayload(SeqLockTestPayload<N>& p, std::uint64_t i) noexcept {
    p.a = i;
    p.b = ~i;
    if constexpr (N > 16) {
        for (std::size_t k = 0; k < N - 16; ++k) {
            p.filler[k] = static_cast<std::uint8_t>(i + k);
        }
    }
}

template <std::size_t N>
bool validPayload(const SeqLockTestPayload<N>& p) noexcept {
    if (p.b != ~p.a) {
        return false;
    }
    if constexpr (N > 16) {
        for (std::size_t k = 0; k < N - 16; ++k) {
            if (p.filler[k] != static_cast<std::uint8_t>(p.a + k)) {
                return false;
            }
        }
    }
    return true;
}

static_assert(sizeof(SeqLockTestPayload<16>)   == 16);
static_assert(sizeof(SeqLockTestPayload<64>)   == 64);
static_assert(sizeof(SeqLockTestPayload<256>)  == 256);
static_assert(sizeof(SeqLockTestPayload<1024>) == 1024);
static_assert(sizeof(SeqLockTestPayload<4096>) == 4096);
static_assert(seqlock_layout_ok_v<SeqLockTestPayload<16>>);
static_assert(seqlock_layout_ok_v<SeqLockTestPayload<64>>);
static_assert(seqlock_layout_ok_v<SeqLockTestPayload<256>>);
static_assert(seqlock_layout_ok_v<SeqLockTestPayload<1024>>);
static_assert(seqlock_layout_ok_v<SeqLockTestPayload<4096>>);

class SeqLockTester {
public:
    bool runAll(std::uint64_t publishes, unsigned readers) {
        std::printf("\n=== SeqLock Tests (publishes=%" PRIu64 ", readers=%u, CACHE_LINE_SIZE=%zu) ===\n",
                    publishes, readers, CACHE_LINE_SIZE);
        discoverCpus();

        bool ok = true;
        ok &= t1RoundTrip<16>();
        ok &= t1RoundTrip<64>();
        ok &= t1RoundTrip<256>();
        ok &= t1RoundTrip<1024>();
        ok &= t1RoundTrip<4096>();

        ok &= t3SequenceSemantics();

        std::printf("\n--- T2/T5 torn-read detection, in-process ---\n");
        std::printf("  %6s  %12s  %12s  %10s  %10s  %8s\n",
                    "bytes", "loads", "retries", "retry/load", "violations", "result");
        ok &= t2TornRead<16>(publishes, readers);
        ok &= t2TornRead<64>(publishes, readers);
        ok &= t2TornRead<256>(publishes, readers);
        ok &= t2TornRead<1024>(publishes, readers);
        ok &= t2TornRead<4096>(publishes, readers);

        ok &= t4SharedMemory<64>(publishes, readers);
        ok &= t4SharedMemory<1024>(publishes, readers);

        std::printf("\n=== SeqLock Tests: %s ===\n\n", ok ? "PASS" : "FAIL");
        return ok;
    }

private:
    std::vector<int> m_cpus; // CPUs this process may run on, in order

    void discoverCpus() {
        cpu_set_t set;
        CPU_ZERO(&set);
        if (::sched_getaffinity(0, sizeof(set), &set) == 0) {
            for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
                if (CPU_ISSET(cpu, &set)) {
                    m_cpus.push_back(cpu);
                }
            }
        }
        std::printf("  Allowed CPUs: %zu", m_cpus.size());
        for (int cpu : m_cpus) {
            std::printf(" %d", cpu);
        }
        std::printf("\n");
    }

    // Thread k (0 = writer, 1.. = readers) gets a distinct CPU when there are
    // enough; otherwise they wrap around and share, which is reported.
    void pinCurrentThread(std::size_t k) const {
        if (m_cpus.size() < 2) {
            return;
        }
        const int cpu = m_cpus[(1 + k) % m_cpus.size()];
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(cpu, &set);
        if (::pthread_setaffinity_np(::pthread_self(), sizeof(set), &set) != 0) {
            std::fprintf(stderr, "  WARNING: could not pin thread %zu to CPU %d\n", k, cpu);
        }
    }

    // ── T1 ──
    template <std::size_t N>
    bool t1RoundTrip() {
        using P = SeqLockTestPayload<N>;
        SeqLock<P> lock;
        bool ok = lock.sequence() == 0;

        P in{};
        fillPayload(in, 0x1234'5678'9abc'def0ULL);
        lock.store(in);

        P out{};
        const auto seq = lock.load(out);
        ok &= seq == 2;
        ok &= lock.sequence() == 2;
        ok &= std::memcmp(&in, &out, sizeof(P)) == 0;
        ok &= validPayload(out);

        std::printf("  T1 round trip %5zu B: seq=%" PRIu64 " sizeof(slot)=%zu  %s\n",
                    N, seq, sizeof(SeqLock<P>), ok ? "PASS" : "FAIL");
        return ok;
    }

    // ── T3 ──
    bool t3SequenceSemantics() {
        using P = SeqLockTestPayload<64>;
        SeqLock<P> lock;
        P p{}, out{};
        bool ok = true;

        fillPayload(p, 1);
        lock.store(p);
        auto last = lock.load(out);
        ok &= last == 2;

        // Same number => nothing new, try_load must not touch `out`.
        P untouched{};
        fillPayload(untouched, 0xffULL);
        ok &= lock.try_load(untouched, last) == last;
        ok &= untouched.a == 0xffULL;

        // Writer runs ahead by three publishes: reader sees a jump of 6 (> 2).
        for (std::uint64_t i = 2; i <= 4; ++i) {
            fillPayload(p, i);
            lock.store(p);
        }
        const auto now = lock.try_load(out, last);
        ok &= now == 8;
        ok &= (now - last) > 2;          // "missed one or more publishes"
        ok &= (now - last) / 2 - 1 == 2;  // exactly two missed
        ok &= out.a == 4 && validPayload(out);
        ok &= (now & 1u) == 0;

        std::printf("  T3 sequence semantics: last=%" PRIu64 " now=%" PRIu64 " missed=%" PRIu64 "  %s\n",
                    last, now, (now - last) / 2 - 1, ok ? "PASS" : "FAIL");
        return ok;
    }

    // ── T2 core: one writer thread on `w`, `readers` threads on `r` (same slot, maybe another mapping) ──
    struct TornReadResult {
        std::uint64_t loads{0};
        std::uint64_t retries{0};
        std::uint64_t violations{0};     // invariant broken: torn copy got through
        std::uint64_t seqViolations{0};  // odd, decreasing, or beyond the writer
        bool          finalOk{true};     // last load after the writer stopped is the last publish
    };

    template <std::size_t N>
    TornReadResult tornRead(SeqLock<SeqLockTestPayload<N>>&       w,
                            const SeqLock<SeqLockTestPayload<N>>& r,
                            std::uint64_t publishes, unsigned readers) {
        using P = SeqLockTestPayload<N>;
        std::atomic<bool>     go{false};
        std::atomic<bool>     done{false};
        std::atomic<unsigned> ready{0};
        std::vector<TornReadResult> results(readers);
        std::vector<std::thread>    threads;

        for (unsigned t = 0; t < readers; ++t) {
            threads.emplace_back([&, t] {
                pinCurrentThread(1 + t);
                TornReadResult& res = results[t];
                P out{};
                std::uint64_t lastSeq = 0;
                ready.fetch_add(1, std::memory_order_acq_rel);
                while (!go.load(std::memory_order_acquire)) {}

                while (!done.load(std::memory_order_acquire)) {
                    const auto seq = r.load(out, res.retries);
                    ++res.loads;
                    // seq == 0: nothing published yet, the payload is the zero-initialised slot.
                    if (seq != 0 && (!validPayload(out) || out.a != seq / 2)) {
                        ++res.violations;
                    }
                    if ((seq & 1u) != 0 || seq < lastSeq || seq > 2 * publishes) {
                        ++res.seqViolations;
                    }
                    lastSeq = seq;
                }
                // Writer finished: the slot must now hold the last publish.
                const auto seq = r.load(out, res.retries);
                ++res.loads;
                res.finalOk = seq == 2 * publishes && out.a == publishes && validPayload(out);
            });
        }

        std::thread writer([&] {
            pinCurrentThread(0);
            while (ready.load(std::memory_order_acquire) < readers) {}
            go.store(true, std::memory_order_release);
            P p{};
            for (std::uint64_t i = 1; i <= publishes; ++i) {
                fillPayload(p, i);
                w.store(p);
            }
            done.store(true, std::memory_order_release);
        });

        writer.join();
        for (auto& t : threads) {
            t.join();
        }

        TornReadResult total;
        for (const auto& res : results) {
            total.loads         += res.loads;
            total.retries       += res.retries;
            total.violations    += res.violations;
            total.seqViolations += res.seqViolations;
            total.finalOk       &= res.finalOk;
        }
        return total;
    }

    static bool report(std::size_t bytes, const TornReadResult& t) {
        const bool ok = t.violations == 0 && t.seqViolations == 0 && t.finalOk;
        std::printf("  %6zu  %12" PRIu64 "  %12" PRIu64 "  %10.4f  %10" PRIu64 "  %8s%s\n",
                    bytes, t.loads, t.retries,
                    t.loads ? static_cast<double>(t.retries) / static_cast<double>(t.loads) : 0.0,
                    t.violations, ok ? "PASS" : "FAIL",
                    t.seqViolations ? "  (sequence violations!)" : (t.finalOk ? "" : "  (final state wrong!)"));
        return ok;
    }

    // ── T2 ──
    template <std::size_t N>
    bool t2TornRead(std::uint64_t publishes, unsigned readers) {
        SeqLock<SeqLockTestPayload<N>> lock;
        return report(N, tornRead<N>(lock, lock, publishes, readers));
    }

    // ── T4: same test, writer and readers on two separate mappings of /dev/shm ──
    template <std::size_t N>
    bool t4SharedMemory(std::uint64_t publishes, unsigned readers) {
        using Slot = SeqLock<SeqLockTestPayload<N>>;
        constexpr const char* name = "/abtedge_seqlock_test";

        std::printf("\n--- T4 shared-memory placement (%zu B payload, two ShmBackend mappings) ---\n", N);

        ShmBackend<ShmMode::Writer> writer;
        if (!writer.open<Slot>(name)) {
            std::printf("  FAIL: writer could not open %s\n", name);
            return false;
        }
        // The mapping is raw bytes: construct the slot in place (seq = 0).
        Slot* w = new (writer.ptr<Slot>()) Slot();

        ShmBackend<ShmMode::Reader> reader;
        if (!reader.open<Slot>(name)) {
            std::printf("  FAIL: reader could not open %s\n", name);
            return false;
        }
        const Slot* r = reader.ptr<Slot>();
        std::printf("  writer view=%p  reader view=%p  mmap=%zu B\n",
                    static_cast<void*>(w), static_cast<const void*>(r), writer.getMmapSize());

        bool ok = r->sequence() == 0;

        // T1 across the mapping.
        SeqLockTestPayload<N> in{}, out{};
        fillPayload(in, 7);
        w->store(in);
        ok &= r->load(out) == 2 && std::memcmp(&in, &out, sizeof(in)) == 0;
        std::printf("  T1 across mapping: %s\n", ok ? "PASS" : "FAIL");

        // T2 across the mapping. The writer already published once, so offset by one.
        std::printf("  %6s  %12s  %12s  %10s  %10s  %8s\n",
                    "bytes", "loads", "retries", "retry/load", "violations", "result");
        new (w) Slot(); // reset seq to 0 so the T2 bookkeeping holds
        ok &= report(N, tornRead<N>(*w, *r, publishes, readers));

        writer.close(); // unlinks
        reader.close();
        return ok;
    }
};

#endif // ABTEDGE_SEQLOCKTESTER_HPP
