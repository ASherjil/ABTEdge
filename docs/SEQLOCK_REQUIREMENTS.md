# SeqLock — requirements

Status: DRAFT 2026-10-22. Owner: asherjil. Scope: a single-writer / multi-reader
sequence lock primitive for ABTEdge, to be developed and benchmarked here first,
then consumed by the KiTS FESA classes (MKController → MKRcps / MKPfn cycle handoff).

## 1. Why

The KiTS controller hands the per-cycle kick settings to the two receiver classes
running in the same process through five FESA `ProxyInterface` calls, each an RDA3
round trip over a local TCP socket. Measured on mkdev30 (deb13x64, CPS 1.2 s cycle,
`docs/NEWCYCLE_LATENCY.md` in the KiTS repo):

| Interval (median, µs) | Production array sizes, n=181 | Small arrays, n=136 |
|---|---|---|
| Controller action (5 proxies) | 1,336 | 1,273 |
| End-to-end NewCycle → MKRcps ready | 1,473 | 1,321 |
| End-to-end NewCycle → MKPfn ready | 219,808 | 1,436 |

The 1.3 ms controller action is the transport floor of the proxy path. The 218 ms
MKPfn number is a separate array-clearing problem and is out of scope here.
Replacing the five proxy calls by one seqlock publish is expected to take the handoff
to well under a microsecond. The remaining end-to-end cost would then be the receivers'
own FESA field writes (about 30 to 110 µs today). INFERRED, to be measured after.

Today ABTEdge has `ShmBackend` (mmap lifecycle) but no lock: the seqlock protocol
only exists inline in `src/test/ShmTester.hpp`, as a half version (single `store(1)`,
no odd/even sequence, no writer-side torn-write guard). This document specifies the
reusable primitive.

## 2. Functional requirements

- **R1 Single writer, N readers.** Exactly one writer thread per slot. Any number of
  reader threads. Readers never block the writer. The writer never waits for readers.
- **R2 Payload constraints.** `T` must satisfy `std::is_trivially_copyable_v<T>` and
  be copyable by `std::memcpy`. Enforce with a concept. No pointers into the payload's
  owner process (the slot may live in shared memory).
- **R3 Protocol.** Sequence counter `std::atomic<std::uint64_t>`:
  - Writer: `seq` += 1 (odd, "write in progress"), copy payload, `seq` += 1 (even,
    "stable"). Both stores `memory_order_release`; the payload copy must not be
    reordered before the first store (needs an acquire or `atomic_thread_fence` after
    the first increment, see §4).
  - Reader: load `seq` (acquire); if odd, retry; copy payload out; load `seq` again;
    if different from the first load, retry. Return the even sequence number read.
- **R4 Reader result.** The reader returns the sequence number it validated. The
  consumer uses it to decide "no new data since last read" (same number) and
  "I missed one or more publishes" (jump by more than 2). Nothing in the primitive
  blocks, sleeps or spins with a bounded retry count; the retry loop is the only loop.
- **R5 Memory-agnostic.** The slot works in any memory: a `static` object in-process
  (KiTS standalone mode, the first use case), a member of a struct placed in
  `ShmBackend` memory, or a hugepage buffer. The primitive must not allocate and must
  not own the memory.
- **R6 Layout.** The slot is `alignas(CACHE_LINE_SIZE)` and `seq` and the payload are
  laid out so a reader's two loads and the memcpy touch the minimum number of cache
  lines. Document the layout with `static_assert` on `alignof` and `sizeof`.
  Decide and document whether `seq` shares a line with the start of the payload
  (fewer lines) or sits on its own line (no false sharing between the counter and a
  payload the writer is mid-way through); measure both if cheap.
- **R7 Initialisation in shared memory.** When the slot is placed in `ShmBackend`
  memory the atomic must be constructed by placement-new, as `ShmBackend::open`
  already does by pointer-to-member. `seq` starts at 0 (even, "nothing published").
- **R8 No dynamic allocation, no exceptions, no system calls** on the write or read
  path. `noexcept` on both.
- **R9 Header-only**, C++20, Apache-2.0 SPDX header like the rest of the repo.
  Location: `src/common/SeqLock.hpp`. No dependency on `ShmBackend.hpp` (the reverse
  dependency is fine).
- **R10 Portable across x86-64 and aarch64.** No inline assembly. Correctness must
  come from the C++ memory model only, so the same header is correct on the
  TSO x86 FECs and the weakly ordered ARM64 FECs (DI/OT). The aarch64 benchmark is
  the one that will catch a missing fence; x86 hides it.

## 3. API sketch (to be refined by the implementer)

```cpp
template <typename T>
concept SeqLockPayload = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;

template <SeqLockPayload T>
class alignas(CACHE_LINE_SIZE) SeqLock {
public:
    using sequence_type = std::uint64_t;

    SeqLock() noexcept = default;          // seq = 0, payload value-initialised
    SeqLock(const SeqLock&) = delete;      // a lock is not copyable
    SeqLock& operator=(const SeqLock&) = delete;

    // Writer side. Single caller.
    void store(const T& value) noexcept;

    // Reader side. Returns the even sequence number the copy was validated against.
    [[nodiscard]] sequence_type load(T& out) const noexcept;

    // Non-blocking peek, for "has anything new been published since seq X?"
    [[nodiscard]] sequence_type sequence() const noexcept;

private:
    std::atomic<sequence_type> m_seq{0};
    T                          m_payload{};
};
```

Open points for the implementer, to be decided and written into the header comment:

- Whether `load` takes `T&` (caller owns storage, no copy on the return path) or
  returns `std::optional<T>` / a `struct { T value; sequence_type seq; }`. Prefer the
  `T&` form on the hot path.
- Whether to offer `try_load(T&, sequence_type expected)` that returns immediately if
  `seq == expected` (nothing new), to avoid the memcpy when the consumer already has
  the data. KiTS receivers would use this on every NewCycle.
- `CACHE_LINE_SIZE`: reuse the definition from `ShmBackend.hpp` by moving it to
  `src/common/` so both headers share it (`hardware_destructive_interference_size`
  with the 64 fallback; note it is 64 on both FEC architectures but check the aarch64
  value the toolchain reports).

## 4. Memory-model notes (the part to get right, not fast)

- Writer first increment: `fetch_add(1, release)` alone does not stop the compiler or
  CPU from hoisting the payload stores above it. Use `store(seq + 1, relaxed)` followed
  by `atomic_thread_fence(memory_order_release)` before the payload copy, or
  `fetch_add(1, acq_rel)`. Choose one and justify it in a comment with the
  happens-before chain written out.
- Reader: `load(acquire)`, memcpy, `atomic_thread_fence(acquire)`, `load(relaxed)` is
  the classic form; `load(acquire)` twice also works and is simpler. Either is fine;
  state which and why.
- The payload copy is a data race in the C++ sense (non-atomic reads concurrent with
  writes). This is the accepted seqlock caveat. Mitigate by copying with `std::memcpy`
  (never reading fields of `m_payload` directly) and keep the payload trivially
  copyable so the torn copy is discarded without ever being interpreted. Document it.
- ARM64 is weakly ordered. Run the stress test (§5) on an aarch64 FEC with at least
  two reader threads pinned to different cores from the writer. If x86 passes and
  aarch64 fails, the fences are wrong.

## 5. Tests (`src/test/SeqLockTester.hpp`, wired into `src/test/main.cpp`)

- **T1 Single-threaded round trip.** store, load, compare, sequence == 2.
- **T2 Torn-read detection.** One writer thread publishing a payload whose fields are
  derived from one counter (e.g. `a = i, b = i * 3, c = ~i`), N reader threads
  validating the invariant on every successful load. Run for a fixed number of
  publishes (e.g. 10 million). Count: successful loads, retries, invariant violations.
  Pass criterion: zero violations. Report the retry rate.
- **T3 Sequence semantics.** Reader sees monotonically non-decreasing even numbers;
  "missed publish" detection works when the writer runs faster than the reader.
- **T4 Shared-memory placement.** Writer process and reader process (or two threads
  with separate `ShmBackend` mappings of the same name) using `ShmBackend::open` with
  the pointer-to-member init of `seq`. Repeat T1 and T2 across the mapping.
- **T5 Payload size sweep.** Payload of 16 B, 64 B, 256 B, 1 KiB, 4 KiB (the KiTS
  per-cycle slot will be on the order of 16 kick settings × a few fields, a few
  hundred bytes). Used by the benchmark too.
- Build the tests at both `-O2` and `-O0 -g`; the seqlock must be correct at both,
  and the KiTS classes are currently built at `-O0`.

## 6. Benchmark (both FEC architectures)

Target machines: an x86-64 FEC (mkdev30, deb13x64) and an aarch64 FEC (a DI/OT with
deb13a64, e.g. the one used for FIDS). Run as root with `mlockall` and `SCHED_FIFO`
as `src/test/main.cpp` already does; pin writer and readers to distinct cores with
`sched_setaffinity`. Record kernel version, CPU model, `isolcpus` if any.

Measure with `std::chrono::steady_clock` (or `rdtsc` / `cntvct_el0` if resolution is
a problem) and aggregate in a histogram (HdrHistogram: the Python package is in the
KiTS analysis venv `/opt/home/asherjil/kits-latency-venv`; for C++ either embed a
simple fixed-bucket histogram or dump raw samples to CSV and post-process with the
same venv). Report min / P50 / P90 / P99 / P99.9 / P99.99 / max and the sample count.

Metrics:

- **B1 Writer `store` cost** per payload size (T5 sizes).
- **B2 Reader `load` cost** uncontended (writer idle), per payload size.
- **B3 Reader `load` cost under contention** (writer publishing continuously),
  including the retry count distribution.
- **B4 Publish-to-visible latency**: writer stamps `steady_clock::now()` into the
  payload just before `store`; a spinning reader on another core stamps when it
  validates a new sequence; latency = reader stamp − payload stamp. This is the
  number that replaces the 1.3 ms proxy path. Expected sub-microsecond; verify.
- **B5 Same as B4 across `ShmBackend` shared memory** between two processes, to
  confirm in-process and /dev/shm are equivalent (expected, since both are plain
  cache-coherent memory once mapped).
- **B6 `-O0` vs `-O2`**, because the first consumer is built at `-O0`.

Deliverable: `docs/SEQLOCK_BENCHMARK.md` with the tables for both architectures,
tagged MEASURED, with the build flags and machine details, plus the raw CSVs under
`docs/benchmarks/`.

## 7. Integration notes for KiTS (later, not part of this work)

- Standalone mode: one `static SeqLock<CycleSlot>` shared by the three classes in the
  deploy unit. The controller's `_prepareNewCycle` writes it; the receivers'
  `CmdNewCycle` read it. No shared memory needed.
- Master/Receiver mode: unchanged, still RDA3 over the network to the remote FEC.
- `CycleSlot` must be trivially copyable: `std::array` for the kick settings
  (`kMaxKickSetting = 16` rows), scalars for the trigger flags, plus the cycle stamp
  so a receiver can reject a stale slot.
- The receivers' existing polling loops (`_waitCycleUnInitialized`, 1 ms sleeps) and
  the zero-valued `usleep` sync delays are replaced by the sequence check; the three
  hardware settling delays stay as they are.
- Header-only consumption: add the ABTEdge include path to the class `Makefile.specific`
  (or install the headers under `ABT_INST_DIR`). ABTEdge is C++20, KiTS is C++23; both
  fine for a header-only primitive.

## 8. Out of scope

- Multi-writer variants, ring buffers, notification/wake-up of a blocked reader
  (the receivers are driven by FESA timing events and only need to read).
- The MKPfn 2-D array clearing (separate ticket).
- Kernel bypass or hugepage placement of the slot (a single cache line in normal
  memory is the whole requirement).
