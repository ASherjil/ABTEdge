//
// Intel I210 Ethernet Controller Register Map
// Reference: Intel I210 Datasheet, Document 333016, Revision 3.7
// Device ID: 0x1533 (I210 Gigabit Network Connection - Copper)
//
// All offsets are relative to BAR0 (Memory-Mapped I/O base address).
// Only Queue 0 registers are defined — queues 1-3 are at +0x40*(n) from these base offsets.
//

#ifndef ABTEDGE_I210REGISTERS_HPP
#define ABTEDGE_I210REGISTERS_HPP

#include <cstdint>

// ============================================================================
// PCI Identity
// ============================================================================
constexpr std::uint16_t VENDOR_ID  = 0x8086;   // Intel Corporation
constexpr std::uint16_t DEVICE_ID  = 0x1533;   // I210 Gigabit Network Connection (Copper)

// ============================================================================
// Section 8.2 — General Registers
// ============================================================================
constexpr std::uint32_t CTRL       = 0x0000;   // Device Control (R/W)
constexpr std::uint32_t STATUS     = 0x0008;   // Device Status (RO)
constexpr std::uint32_t CTRL_EXT   = 0x0018;   // Extended Device Control (R/W)
constexpr std::uint32_t MDIC       = 0x0020;   // MDI Control (R/W) — PHY register access
constexpr std::uint32_t CONNSW     = 0x0034;   // Copper/Fiber Switch Control (R/W)
constexpr std::uint32_t VET        = 0x0038;   // VLAN Ether Type (R/W)
constexpr std::uint32_t LEDCTL     = 0x0E00;   // LED Control (R/W)

// ============================================================================
// Section 8.2.1 — CTRL Register Bit Definitions
// ============================================================================
constexpr std::uint32_t CTRL_FD         = (1U << 0);    // Full Duplex
constexpr std::uint32_t CTRL_GIO_MD     = (1U << 2);    // GIO Master Disable
constexpr std::uint32_t CTRL_SLU        = (1U << 6);    // Set Link Up
constexpr std::uint32_t CTRL_FRCSPD     = (1U << 11);   // Force Speed
constexpr std::uint32_t CTRL_FRCDPLX    = (1U << 12);   // Force Duplex
constexpr std::uint32_t CTRL_RST        = (1U << 26);   // Software Reset (self-clearing)
constexpr std::uint32_t CTRL_VME        = (1U << 30);   // VLAN Mode Enable
constexpr std::uint32_t CTRL_PHY_RST    = (1U << 31);   // PHY Reset
constexpr std::uint32_t CTRL_DEV_RST    = (1U << 29);   // Device Reset (self-clearing)

// ============================================================================
// Section 8.2.2 — STATUS Register Bit Definitions
// ============================================================================
constexpr std::uint32_t STATUS_FD       = (1U << 0);    // Full Duplex indication
constexpr std::uint32_t STATUS_LU       = (1U << 1);    // Link Up indication
constexpr std::uint32_t STATUS_TXOFF    = (1U << 4);    // Transmission Paused
constexpr std::uint32_t STATUS_SPEED_SHIFT = 6;
constexpr std::uint32_t STATUS_SPEED_MASK  = (0x3U << STATUS_SPEED_SHIFT); // bits [7:6]
// 00b = 10 Mb/s, 01b = 100 Mb/s, 10b = 1000 Mb/s, 11b = 1000 Mb/s

// ============================================================================
// Section 8.8 — Interrupt Registers
// We disable all interrupts since we use busy-polling for lowest latency.
// ============================================================================
constexpr std::uint32_t EICR       = 0x1580;   // Extended Interrupt Cause Read (RC/W1C)
constexpr std::uint32_t EICS       = 0x1520;   // Extended Interrupt Cause Set (WO)
constexpr std::uint32_t EIMS       = 0x1524;   // Extended Interrupt Mask Set/Read (RWM)
constexpr std::uint32_t EIMC       = 0x1528;   // Extended Interrupt Mask Clear (WO)
constexpr std::uint32_t EIAC       = 0x152C;   // Extended Interrupt Auto Clear (R/W)
constexpr std::uint32_t EIAM       = 0x1530;   // Extended Interrupt Auto Mask Enable (R/W)
constexpr std::uint32_t ICR        = 0x1500;   // Interrupt Cause Read (RC/W1C)
constexpr std::uint32_t ICS        = 0x1504;   // Interrupt Cause Set (WO)
constexpr std::uint32_t IMS        = 0x1508;   // Interrupt Mask Set/Read (R/W)
constexpr std::uint32_t IMC        = 0x150C;   // Interrupt Mask Clear (WO)
constexpr std::uint32_t IAM        = 0x1510;   // Interrupt Acknowledge Auto-mask (R/W)
constexpr std::uint32_t GPIE       = 0x1514;   // General Purpose Interrupt Enable (RW)
constexpr std::uint32_t EITR0      = 0x1680;   // Interrupt Throttle Register 0 (R/W)
// EITR[n] = 0x1680 + 4*n, n = 0..4
constexpr std::uint32_t IVAR0      = 0x1700;   // Interrupt Vector Allocation 0 (RW)
constexpr std::uint32_t IVAR_MISC  = 0x1740;   // Interrupt Vector Allocation Misc (RW)

// ============================================================================
// Section 8.3 — Internal Packet Buffer Size Registers
// ============================================================================
constexpr std::uint32_t RXPBSIZE   = 0x2404;   // Rx Packet Buffer Size (R/W)
constexpr std::uint32_t TXPBSIZE   = 0x3404;   // Tx Packet Buffer Size (R/W)

// ============================================================================
// Section 8.10 — Receive Registers (Queue 0)
// ============================================================================

// -- Receive Control --
constexpr std::uint32_t RCTL       = 0x0100;   // Receive Control Register (R/W)

// RCTL Bit Definitions (Section 8.10.1)
constexpr std::uint32_t RCTL_RXEN      = (1U << 1);    // Receiver Enable
constexpr std::uint32_t RCTL_SBP       = (1U << 2);    // Store Bad Packets
constexpr std::uint32_t RCTL_UPE       = (1U << 3);    // Unicast Promiscuous Enable
constexpr std::uint32_t RCTL_MPE       = (1U << 4);    // Multicast Promiscuous Enable
constexpr std::uint32_t RCTL_LPE       = (1U << 5);    // Long Packet Reception Enable
constexpr std::uint32_t RCTL_BAM       = (1U << 15);   // Broadcast Accept Mode
constexpr std::uint32_t RCTL_BSIZE_2048 = (0U << 16);  // Buffer Size 2048 bytes (default)
constexpr std::uint32_t RCTL_BSIZE_1024 = (1U << 16);  // Buffer Size 1024 bytes
constexpr std::uint32_t RCTL_BSIZE_512  = (2U << 16);  // Buffer Size 512 bytes
constexpr std::uint32_t RCTL_BSIZE_256  = (3U << 16);  // Buffer Size 256 bytes
constexpr std::uint32_t RCTL_VFE       = (1U << 18);   // VLAN Filter Enable
constexpr std::uint32_t RCTL_DPF       = (1U << 22);   // Discard Pause Frames
constexpr std::uint32_t RCTL_SECRC     = (1U << 26);   // Strip Ethernet CRC from incoming packet

// -- Rx Descriptor Ring Registers (Queue 0) --
constexpr std::uint32_t RDBAL0     = 0xC000;   // Rx Descriptor Base Address Low (R/W)
constexpr std::uint32_t RDBAH0     = 0xC004;   // Rx Descriptor Base Address High (R/W)
constexpr std::uint32_t RDLEN0     = 0xC008;   // Rx Descriptor Ring Length in bytes (R/W) — must be 128-byte aligned
constexpr std::uint32_t SRRCTL0    = 0xC00C;   // Split and Replication Receive Control (R/W)
constexpr std::uint32_t RDH0       = 0xC010;   // Rx Descriptor Head (RO)
constexpr std::uint32_t RDT0       = 0xC018;   // Rx Descriptor Tail (R/W)
constexpr std::uint32_t RXDCTL0    = 0xC028;   // Rx Descriptor Control (R/W)
constexpr std::uint32_t RQDPC0     = 0xC030;   // Rx Queue Drop Packet Count (RW)

// RXDCTL Bit Definitions (Section 8.10.9)
constexpr std::uint32_t RXDCTL_ENABLE  = (1U << 25);   // Receive Queue Enable
constexpr std::uint32_t RXDCTL_SWFLUSH = (1U << 26);   // Receive Software Flush

// SRRCTL Bit Definitions (Section 8.10.2)
constexpr std::uint32_t SRRCTL_BSIZEPACKET_SHIFT = 0;  // Buffer size in 1KB units (bits [6:0])
constexpr std::uint32_t SRRCTL_DESCTYPE_SHIFT    = 25; // Descriptor type (bits [27:25])
constexpr std::uint32_t SRRCTL_DESCTYPE_LEGACY   = (0U << 25);  // 000b = Legacy
constexpr std::uint32_t SRRCTL_DESCTYPE_ADV_ONE  = (1U << 25);  // 001b = Advanced, one buffer
constexpr std::uint32_t SRRCTL_DROP_EN           = (1U << 31);  // Drop Enable

// -- Receive Filtering --
constexpr std::uint32_t RXCSUM     = 0x5000;   // Receive Checksum Control (R/W)
constexpr std::uint32_t RLPML      = 0x5004;   // Receive Long Packet Max Length (R/W)
constexpr std::uint32_t RFCTL      = 0x5008;   // Receive Filter Control (R/W)
constexpr std::uint32_t MTA        = 0x5200;   // Multicast Table Array (R/W) — 128 entries, +4*n
constexpr std::uint32_t RAL0       = 0x5400;   // Receive Address Low 0 (R/W) — MAC address [31:0]
constexpr std::uint32_t RAH0       = 0x5404;   // Receive Address High 0 (R/W) — MAC address [47:32]
// RAL[n] = 0x5400 + 8*n, RAH[n] = 0x5404 + 8*n, n = 0..15

// RAH Bit Definitions (Section 8.10.17)
constexpr std::uint32_t RAH_AV         = (1U << 31);   // Address Valid
constexpr std::uint32_t RAH_ASEL_DEST  = (0U << 16);   // 00b = Destination address (normal)

constexpr std::uint32_t MRQC       = 0x5818;   // Multiple Receive Queues Command (R/W)

// ============================================================================
// Section 8.12 — Transmit Registers (Queue 0)
// ============================================================================

// -- Transmit Control --
constexpr std::uint32_t TCTL       = 0x0400;   // Transmit Control Register (R/W)
constexpr std::uint32_t TCTL_EXT   = 0x0404;   // Transmit Control Extended (R/W)
constexpr std::uint32_t TIPG       = 0x0410;   // Transmit IPG Register (R/W)

// TCTL Bit Definitions (Section 8.12.1)
constexpr std::uint32_t TCTL_EN        = (1U << 1);    // Transmit Enable
constexpr std::uint32_t TCTL_PSP       = (1U << 3);    // Pad Short Packets (to 64 bytes)
constexpr std::uint32_t TCTL_CT_SHIFT  = 4;            // Collision Threshold (bits [11:4])
constexpr std::uint32_t TCTL_CT_IEEE   = (0xFU << 4);  // IEEE 802.3 value = 15
constexpr std::uint32_t TCTL_BST_SHIFT = 12;           // Back-Off Slot Time (bits [21:12])
constexpr std::uint32_t TCTL_BST_DEF   = (0x40U << 12);// Default = 64 byte times
constexpr std::uint32_t TCTL_RTLC      = (1U << 24);   // Re-transmit on Late Collision

// -- Tx Descriptor Ring Registers (Queue 0) --
constexpr std::uint32_t TDBAL0     = 0xE000;   // Tx Descriptor Base Address Low (R/W)
constexpr std::uint32_t TDBAH0     = 0xE004;   // Tx Descriptor Base Address High (R/W)
constexpr std::uint32_t TDLEN0     = 0xE008;   // Tx Descriptor Ring Length in bytes (R/W) — must be 128-byte aligned
constexpr std::uint32_t TDH0       = 0xE010;   // Tx Descriptor Head (RO)
constexpr std::uint32_t TDT0       = 0xE018;   // Tx Descriptor Tail (R/W)
constexpr std::uint32_t TXDCTL0    = 0xE028;   // Tx Descriptor Control (R/W)
constexpr std::uint32_t TDWBAL0    = 0xE038;   // Tx Descriptor Completion Write-Back Address Low (R/W)
constexpr std::uint32_t TDWBAH0    = 0xE03C;   // Tx Descriptor Completion Write-Back Address High (R/W)

// TXDCTL Bit Definitions (Section 8.12.15)
constexpr std::uint32_t TXDCTL_PTHRESH_SHIFT  = 0;     // Prefetch Threshold (bits [4:0])
constexpr std::uint32_t TXDCTL_HTHRESH_SHIFT  = 8;     // Host Threshold (bits [12:8])
constexpr std::uint32_t TXDCTL_WTHRESH_SHIFT  = 16;    // Write-Back Threshold (bits [20:16])
constexpr std::uint32_t TXDCTL_ENABLE         = (1U << 25);  // Transmit Queue Enable
constexpr std::uint32_t TXDCTL_SWFLSH         = (1U << 26);  // Transmit Software Flush
constexpr std::uint32_t TXDCTL_PRIORITY       = (1U << 27);  // Transmit Queue Priority

// -- Transmit DMA --
constexpr std::uint32_t DTXCTL     = 0x3590;   // DMA Tx Control (R/W)
constexpr std::uint32_t DTXMXPKTSZ = 0x355C;   // DMA Tx Maximum Packet Size (RW)
constexpr std::uint32_t DTXMXSZRQ  = 0x3540;   // DMA Tx Max Outstanding Requests (RW)
constexpr std::uint32_t RETX_CTL   = 0x041C;   // Retry Buffer Control (RW)
constexpr std::uint32_t TQAVCTRL   = 0x3570;   // Tx Qav Control (R/W) — TransmitMode bit 0

// ============================================================================
// Section 8.7 — Semaphore Registers
// ============================================================================
constexpr std::uint32_t SWSM       = 0x5B50;   // Software Semaphore (R/W)
constexpr std::uint32_t FWSM       = 0x5B54;   // Firmware Semaphore (RO to Host)
constexpr std::uint32_t SW_FW_SYNC = 0x5B5C;   // Software-Firmware Synchronization (RWM)

// ============================================================================
// Section 8.6 — PCIe Registers
// ============================================================================
constexpr std::uint32_t GCR        = 0x5B00;   // PCIe Control (RW)
constexpr std::uint32_t GCR_EXT    = 0x5B6C;   // PCIe Control Extended (RW)

// ============================================================================
// Section 8.4 — EEPROM/Flash Registers
// ============================================================================
constexpr std::uint32_t EEC        = 0x12010;  // EEPROM-Mode Control (RW)
constexpr std::uint32_t EERD       = 0x12014;  // EEPROM-Mode Read (RW)

// ============================================================================
// Legacy Rx Descriptor (16 bytes) — Section 7.1.3
//
// Read format (software writes before giving to hardware):
//   [63:0]  Buffer Address  — physical address where NIC will DMA the packet
//   [127:64] zeroed         — hardware fills this on write-back
//
// Write-back format (hardware fills after packet arrives):
//   [63:0]  Buffer Address  — unchanged
//   [79:64] Length           — bytes received
//   [95:80] Checksum         — packet checksum
//   [103:96] Status          — DD (bit 0) = Descriptor Done
//   [111:104] Errors         — receive errors
//   [127:112] VLAN Tag       — VLAN tag (if stripped)
// ============================================================================
struct RxDescriptor {
    std::uint64_t bufferAddr;     // Physical address of the receive buffer
    std::uint16_t length;         // Number of bytes received (written by hardware)
    std::uint16_t checksum;       // Packet checksum (written by hardware)
    std::uint8_t  status;         // Status bits (written by hardware)
    std::uint8_t  errors;         // Error bits (written by hardware)
    std::uint16_t vlanTag;        // VLAN tag (written by hardware)
};
static_assert(sizeof(RxDescriptor) == 16, "RxDescriptor must be exactly 16 bytes");

// RxDescriptor Status Bit Definitions (Section 7.1.3.2)
constexpr std::uint8_t RXD_STAT_DD     = (1U << 0);   // Descriptor Done
constexpr std::uint8_t RXD_STAT_EOP    = (1U << 1);   // End of Packet
constexpr std::uint8_t RXD_STAT_VP     = (1U << 3);   // VLAN Packet (802.1Q match)
constexpr std::uint8_t RXD_STAT_UDPCS  = (1U << 4);   // UDP Checksum calculated
constexpr std::uint8_t RXD_STAT_TCPCS  = (1U << 5);   // TCP Checksum calculated
constexpr std::uint8_t RXD_STAT_IPCS   = (1U << 6);   // IP Checksum calculated

// RxDescriptor Error Bit Definitions (Section 7.1.3.3)
constexpr std::uint8_t RXD_ERR_CE      = (1U << 0);   // CRC Error / Alignment Error
constexpr std::uint8_t RXD_ERR_SE      = (1U << 1);   // Symbol Error
constexpr std::uint8_t RXD_ERR_SEQ     = (1U << 2);   // Sequence Error
constexpr std::uint8_t RXD_ERR_RXE     = (1U << 7);   // Rx Data Error

// ============================================================================
// Legacy Tx Descriptor (16 bytes) — Section 7.2.2.1
//
//   [63:0]   Buffer Address   — physical address of packet data in host memory
//   [79:64]  Length            — bytes to transmit from buffer
//   [87:80]  CSO              — Checksum Offset
//   [95:88]  CMD              — Command field
//   [103:96] STA/RSV          — Status (DD bit) / Reserved
//   [111:104] CSS             — Checksum Start
//   [127:112] VLAN            — VLAN tag (if VLE set in CMD)
// ============================================================================
struct TxDescriptor {
    std::uint64_t bufferAddr;     // Physical address of the transmit data
    std::uint16_t length;         // Length of data in bytes
    std::uint8_t  cso;            // Checksum Offset
    std::uint8_t  cmd;            // Command byte
    std::uint8_t  sta;            // Status byte (DD bit 0, written by hardware)
    std::uint8_t  css;            // Checksum Start
    std::uint16_t vlanTag;        // VLAN tag
};
static_assert(sizeof(TxDescriptor) == 16, "TxDescriptor must be exactly 16 bytes");

// TxDescriptor CMD Bit Definitions (Section 7.2.2.1.4)
constexpr std::uint8_t TXD_CMD_EOP     = (1U << 0);   // End of Packet
constexpr std::uint8_t TXD_CMD_IFCS    = (1U << 1);   // Insert FCS (CRC)
constexpr std::uint8_t TXD_CMD_IC      = (1U << 2);   // Insert Checksum
constexpr std::uint8_t TXD_CMD_RS      = (1U << 3);   // Report Status (sets DD on completion)
constexpr std::uint8_t TXD_CMD_DEXT    = (1U << 5);   // Descriptor Extension (0 = legacy)
constexpr std::uint8_t TXD_CMD_VLE     = (1U << 6);   // VLAN Packet Enable
constexpr std::uint8_t TXD_CMD_IDE     = (1U << 7);   // Interrupt Delay Enable

// TxDescriptor STA Bit Definitions (Section 7.2.2.1.5)
constexpr std::uint8_t TXD_STAT_DD     = (1U << 0);   // Descriptor Done

// ============================================================================
// Typical TIPG values for 1000BASE-T (Section 8.12.3)
// IPGT = 8, IPGR1 = 8 (2/3 of 12), IPGR = 6
// ============================================================================
constexpr std::uint32_t TIPG_DEFAULT = (8U << 0) | (8U << 10) | (6U << 20);

// ============================================================================
// Convenience: disable all interrupts
// Write 0xFFFFFFFF to IMC and EIMC to mask everything
// ============================================================================
constexpr std::uint32_t IRQ_DISABLE_ALL = 0xFFFFFFFF;

#endif //ABTEDGE_I210REGISTERS_HPP
