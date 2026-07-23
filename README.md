# ABTEdge

Low latency, kernel-bypass C++20 library for direct hardware register access over **PCIe** and **AXI** buses on Linux — plus the **DMA building blocks** (physically contiguous hugepage memory and typed descriptor rings) needed to build complete userspace device drivers on top of it.

No kernel drivers. No `ioctl` overhead. No bloated abstractions. Just `mmap` straight into hardware and pointer dereferences that compile down to single CPU load/store instructions.

## Why this exists

If you need to read/write registers on a PCIe device or an FPGA SoC, your options today are:

1. **Write a kernel driver** — months of work, fragile, kernel-version-dependent
2. **Use vendor-specific drivers** — locked to one product, often closed-source, bloated APIs
3. **Use an existing `mmap` wrapper from GitHub** — legacy C, raw `void*` pointers everywhere, manual `munmap` cleanup, no type safety

ABTEdge gives you a clean C++20 interface with RAII resource management, type-safe templated register access, and zero runtime overhead. The `mmap` lifecycle, file descriptors, and pointer arithmetic are fully abstracted — you work with named registers and typed pointers.

Two capabilities compose into a full userspace driver:

* **MMIO** — the CPU reaches *into the device*: register reads/writes over a PCIe BAR or an AXI physical window.
* **DMA** — the device reaches *into host memory*: descriptor rings and packet/data buffers the hardware reads and writes directly, built on hugepage-backed physically contiguous allocations.

```
Userspace process
    │
    ├─ MMIO (CPU → device registers)
    │   ├─ PCIeBackend ──► mmap("/sys/bus/pci/devices/.../resourceN")
    │   │                  BAR auto-discovered via vendor/device ID
    │   └─ AXIBackend  ──► mmap("/dev/mem", physicalAddress)
    │                      Direct physical address for ARM/FPGA SoCs
    │
    └─ DMA (device → host memory)
        ├─ HugepageBuffer ──► 2 MiB hugepage, locked, physically contiguous
        │                     virtualAddr() for the CPU, physicalAddr() for the device
        └─ DMARing<Desc>  ──► typed descriptor ring on a HugepageBuffer
                              physicalBase() goes into the device's ring-base register
```

## Requirements

- Linux with `mmap` support for device resources
- C++20 compiler (GCC 11+ or Clang 14+)
- Root privileges (or appropriate `/dev/mem` / sysfs permissions)
- CMake 3.21+
- For DMA: reserved 2 MiB hugepages (`echo 64 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages`)

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Platform is auto-detected (`x86_64` or `aarch64`). Override with `-DABTEDGE_ARCH=X86_64` or `-DABTEDGE_ARCH=AARCH64`.

## MMIO API

### 1. Open a backend

**PCIe** — auto-discovers the device by scanning `/sys/bus/pci/devices/` for a matching vendor/device ID, then maps the chosen BAR through its sysfs `resourceN` file (which the kernel maps uncached, so every access really reaches the wire):

```cpp
#include "backends/PCIeBackend.hpp"

PCIeBackend pcie(0x10DC, 0x0455, /*bar=*/1);  // vendor, device, BAR index
if (!pcie.open()) { /* handle error */ }
```

**AXI** — maps a known physical address range through `/dev/mem`. This is the SoC/FPGA path (Zynq-class parts): the programmable logic's registers live at fixed physical addresses on the AXI interconnect, and one `open()` gives you a typed window onto them:

```cpp
#include "backends/AXIBackend.hpp"

AXIBackend axi;
if (!axi.open("/dev/mem", 0xB0020000, 0x1000)) { /* handle error */ }
```

Both backends satisfy the same C++20 concept (`common/BackendConcept.hpp`), so driver code can be written once and templated over the bus type.

### 2. Define your register map

Define your registers as constants — no magic hex numbers scattered through your code:

```cpp
// Your device's register offsets
constexpr std::size_t STATUS_REG     = 0x00;
constexpr std::size_t CONTROL_REG    = 0x04;
constexpr std::size_t DATA_REG       = 0x08;
constexpr std::size_t FIRMWARE_VER   = 0x0C;
constexpr std::size_t INTERRUPT_MASK = 0x30;

// Bitfield masks for STATUS_REG
constexpr std::uint32_t STATUS_LINK_UP    = 0x00000001;  // bit 0
constexpr std::uint32_t STATUS_TIME_VALID = 0x00000002;  // bit 1
constexpr std::uint32_t STATUS_ERROR_CODE = 0x0000FF00;  // bits [15:8]
```

### 3. Read and write registers

#### 32-bit read/write

```cpp
// Read a 32-bit register
std::uint32_t status = *backend.registerPtr<std::uint32_t>(STATUS_REG);

// Write a 32-bit register
*backend.registerPtr<std::uint32_t>(CONTROL_REG) = 0xDEADBEEF;
```

#### 64-bit read/write

Read two adjacent 32-bit registers in a single bus transaction:

```cpp
std::uint64_t pair = *backend.registerPtr<std::uint64_t>(STATUS_REG);  // reads 0x00 + 0x04
std::uint32_t lo = static_cast<std::uint32_t>(pair);         // STATUS_REG
std::uint32_t hi = static_cast<std::uint32_t>(pair >> 32);   // CONTROL_REG
```

#### Bitfield read — `readFromField<Mask>(offset)`

Extracts a field from a register using a compile-time mask. Reads the register, ANDs with the mask, and shifts the result to bit 0:

```cpp
// Extract single-bit flag
std::uint32_t linkUp = backend.readFromField<STATUS_LINK_UP>(STATUS_REG);    // 0 or 1

// Extract multi-bit field (bits [15:8] → shifted to [7:0])
std::uint32_t errCode = backend.readFromField<STATUS_ERROR_CODE>(STATUS_REG); // 0x00..0xFF
```

#### Bitfield write — `writeToField<Mask>(offset, value)`

Read-modify-write: preserves all bits outside the mask, writes `value` into the masked position:

```cpp
constexpr std::uint32_t CTRL_ENABLE = 0x00000080;  // bit 7

backend.writeToField<CTRL_ENABLE>(CONTROL_REG, 1);  // sets bit 7, preserves everything else
backend.writeToField<CTRL_ENABLE>(CONTROL_REG, 0);  // clears bit 7, preserves everything else
```

### 4. Status queries

```cpp
backend.isOpen();          // true after successful open()
backend.getBaseAddress();  // void* to the mmap'd region
backend.getMmapSize();     // size in bytes of the mapped region
```

### 5. Lifecycle

Backends are **move-only** (no copy). RAII handles everything — destruction automatically calls `munmap` and closes the file descriptor. No manual cleanup required.

```cpp
PCIeBackend a(0x10DC, 0x0455, 1);
PCIeBackend b = std::move(a);  // a is now empty, b owns the mapping
// b unmaps automatically on destruction
```

## MMIO example

```cpp
#include "backends/PCIeBackend.hpp"

// Device identification
constexpr std::uint16_t MY_VENDOR = 0x1234;
constexpr std::uint16_t MY_DEVICE = 0x5678;
constexpr int MY_BAR = 0;

// Register map
constexpr std::size_t IDENT_REG    = 0x00;
constexpr std::size_t STATUS_REG   = 0x04;
constexpr std::size_t CONTROL_REG  = 0x08;
constexpr std::size_t TX_DATA_REG  = 0x10;

// Bitfield masks
constexpr std::uint32_t STATUS_READY = 0x00000001;
constexpr std::uint32_t CTRL_GO      = 0x00000001;
constexpr std::uint32_t CTRL_RESET   = 0x80000000;

int main() {
    PCIeBackend hw(MY_VENDOR, MY_DEVICE, MY_BAR);
    if (!hw.open()) return 1;

    // Verify device identity
    std::uint32_t ident = *hw.registerPtr<std::uint32_t>(IDENT_REG);

    // Poll until ready
    while (!hw.readFromField<STATUS_READY>(STATUS_REG)) {}

    // Write data and trigger
    *hw.registerPtr<std::uint32_t>(TX_DATA_REG) = 0xCAFEBABE;
    hw.writeToField<CTRL_GO>(CONTROL_REG, 1);

    return 0;
}   // hw unmaps and closes automatically
```

## DMA API

MMIO lets the CPU reach the device; DMA lets the device reach host memory — that is how any real NIC, capture card, or accelerator moves bulk data. The device needs two things from you: memory it can address **physically**, and a **descriptor ring** telling it where that memory is. ABTEdge provides both.

### `HugepageBuffer` — physically contiguous, device-addressable memory

Allocates from 2 MiB hugepages (`MAP_HUGETLB | MAP_LOCKED`): one hugepage is physically contiguous by construction, locked so it can never be swapped or migrated while the device is mid-transfer, and its physical address is resolved once at allocation. You get both views of the same memory — the virtual pointer for CPU access and the physical address to program into the device:

```cpp
#include "backends/HugepageBuffer.hpp"

HugepageBuffer buf;
if (!buf.allocate(2 * 1024 * 1024)) { /* hugepage pool empty? */ }

void*         cpu  = buf.virtualAddr();          // CPU reads/writes here
std::uint64_t dev  = buf.physicalAddr();         // device DMAs here
std::uint64_t slot = buf.physicalAddrAt(0x800);  // physical address at an offset
auto*         hdr  = buf.ptrAt<std::uint32_t>(0x800);  // typed CPU pointer, same offset
```

Move-only RAII, like the MMIO backends — deallocation is automatic.

### `DMARing<Descriptor>` — a typed descriptor ring

A descriptor ring is the contract between driver and device: an array of fixed-layout structs in DMA-visible memory, with head/tail indices exchanged through MMIO registers. `DMARing` is that array, allocated on a `HugepageBuffer` and indexed with full type safety (the descriptor type is enforced trivially copyable at compile time — anything else could not be shared with hardware):

```cpp
#include "backends/DMARing.hpp"

struct RxDescriptor {          // layout defined by YOUR device's datasheet
    std::uint64_t bufferAddr;
    std::uint32_t lengthFlags;
    std::uint32_t status;
};

DMARing<RxDescriptor> ring;
if (!ring.allocate(256)) { /* ... */ }

ring[7].bufferAddr = packets.physicalAddrAt(7 * 2048);  // point desc 7 at a packet slot
ring[7].status     = 0;
std::uint64_t base = ring.physicalBase();  // -> device's RING_BASE register, via MMIO
```

### The universal recipe

Every DMA-capable device is driven with the same five steps, MMIO and DMA interlocking:

1. **Allocate** data buffers (`HugepageBuffer`) and the descriptor ring (`DMARing<Desc>`).
2. **Fill** descriptors with the buffers' physical addresses (`physicalAddrAt`).
3. **Program** the device's ring-base/length registers with `ring.physicalBase()` — via MMIO.
4. **Ring the doorbell** — write the tail index to its MMIO register; the device starts DMA-ing.
5. **Poll completion** — the device writes status back into descriptors (or buffers); the CPU spins on that memory. No interrupts anywhere.

## Real-world example: a userspace NIC driver in one header

The ABTRDA3 low-latency benchmarking suite contains a complete, from-scratch userspace driver for the **Intel I210 gigabit NIC** built entirely on ABTEdge. It is the best reference for how the pieces compose:

| ABTEdge component | What the I210 driver uses it for |
|---|---|
| `PCIeBackend` | maps BAR0; every NIC register (ring bases, heads/tails, control) via `registerPtr`/`writeToField` |
| `HugepageBuffer` | packet buffer memory the NIC DMAs frames into and out of |
| `DMARing<Desc>` | the RX and TX descriptor rings (descriptor layouts from the Intel datasheet) |
| `InterfaceDiscovery` | resolves a Linux interface name to the PCI device behind it |

The register map (`I210Registers.hpp`) was written from the Intel I210 datasheet (document 333016) — demonstrating that a clean-room register header plus ABTEdge is all a userspace PMD needs: no kernel driver, no vendor SDK, and round-trip latencies far below the kernel network stack on identical hardware.

See: https://github.com/ASherjil/ABTRDA3/tree/master

The same pattern ported to an AXI device is the FPGA/SoC story: swap `PCIeBackend` for `AXIBackend`, keep everything else.

## Project structure

```
src/
├── backends/
│   ├── BackendBase.hpp/cpp        — mmap lifecycle, register access templates
│   ├── PCIeBackend.hpp            — PCIe BAR discovery via sysfs (MMIO)
│   ├── AXIBackend.hpp             — AXI /dev/mem mapping, header-only (MMIO)
│   ├── HugepageBuffer.hpp/cpp     — 2 MiB hugepage alloc, virtual + physical views (DMA)
│   ├── DMARing.hpp                — typed descriptor ring on a HugepageBuffer (DMA)
│   ├── ShmBackend.hpp             — shared-memory backend (register file over shm)
│   ├── InterfaceDiscovery.hpp/cpp — Linux netdev name → PCI device resolution
│   └── VendorDeviceDiscovery.hpp/cpp — sysfs scan by vendor/device ID
├── common/
│   ├── BackendConcept.hpp         — C++20 concept constraining the backend interface
│   ├── PCIeDiscoveryConcept.hpp   — concept for discovery providers
│   └── HugePageHelpers.hpp        — hugepage pool checks/reservation helpers
└── x86_64Tuner.hpp/cpp            — low-latency host tuning for busy-poll workloads (x86_64)
```

## License

ABTEdge is licensed under the **Apache License 2.0** — see [LICENSE](LICENSE). Individual files carry SPDX identifiers. One test configuration snippet (`src/test/CMakeLists.txt`) originates from CERN under the Zlib license and retains its original notice; see [NOTICE](NOTICE).
