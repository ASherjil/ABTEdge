# ABTEdge (Accelerator Beam Transfer Encore Driver GEnerator)

Ultra-low latency, kernel-bypass C++20 library for direct hardware register access over **PCIe** and **AXI** buses on Linux.

No kernel drivers. No `ioctl` overhead. No bloated abstractions. Just `mmap` straight into hardware and pointer dereferences that compile down to single CPU load/store instructions.

## Why this exists

If you need to read/write registers on a PCIe device or an FPGA SoC, your options today are:

1. **Write a kernel driver** — months of work, fragile, kernel-version-dependent
2. **Use vendor-specific drivers** — locked to one product, often closed-source, bloated APIs
3. **Use an existing `mmap` wrapper from GitHub** — legacy C, raw `void*` pointers everywhere, manual `munmap` cleanup, no type safety

ABTEdge gives you a clean C++20 interface with RAII resource management, type-safe templated register access, and zero runtime overhead. The `mmap` lifecycle, file descriptors, and pointer arithmetic are fully abstracted — you work with named registers and typed pointers.

```
Userspace process
    │
    ├─ PCIeBackend ──► mmap("/sys/bus/pci/devices/.../resourceN")
    │                  BAR auto-discovered via vendor/device ID
    │
    └─ AXIBackend  ──► mmap("/dev/mem", physicalAddress)
    │                  Direct physical address for ARM/FPGA SoCs
    ▼
volatile T* ──► CPU load/store ──► hardware register
```

## Requirements

- Linux with `mmap` support for device resources
- C++20 compiler (GCC 11+ or Clang 14+)
- Root privileges (or appropriate `/dev/mem` / sysfs permissions)
- CMake 3.21+

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Platform is auto-detected (`x86_64` or `aarch64`). Override with `-DABTEDGE_ARCH=X86_64` or `-DABTEDGE_ARCH=AARCH64`.

## API

### 1. Open a backend

**PCIe** — auto-discovers the device by scanning `/sys/bus/pci/devices/` for a matching vendor/device ID:

```cpp
#include "backends/PCIeBackend.hpp"

PCIeBackend pcie(0x10DC, 0x0455, /*bar=*/1);  // vendor, device, BAR index
if (!pcie.open()) { /* handle error */ }
```

**AXI** — maps a known physical address range through `/dev/mem`:

```cpp
#include "backends/AXIBackend.hpp"

AXIBackend axi;
if (!axi.open("/dev/mem", 0xB0020000, 0x1000)) { /* handle error */ }
```

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

## Full example

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

## Project structure

```
src/
├── backends/
│   ├── BackendBase.hpp/cpp   — mmap lifecycle, register access templates
│   ├── PCIeBackend.hpp/cpp   — PCIe BAR discovery via sysfs
│   └── AXIBackend.hpp        — AXI /dev/mem mapping (header-only)
└── common/
    └── BackendConcept.hpp    — C++20 concept constraining the backend interface
```
