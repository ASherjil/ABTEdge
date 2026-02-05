#!/usr/bin/env python3
"""
EDGE CSV Register Map → C++ Header Generator

Parses CERN EDGE driver generator CSV files and produces a C++ header
with constexpr register offsets, masks, and access mode annotations.

Usage:
    python3 edge_to_hpp.py <input.csv> [output.hpp]
    python3 edge_to_hpp.py registers.csv                    # outputs registers.hpp
    python3 edge_to_hpp.py registers.csv my_regs.hpp        # outputs my_regs.hpp
"""

import sys
import csv
import re
import os
from dataclasses import dataclass
from datetime import datetime
from typing import Optional
from pathlib import Path


@dataclass
class Register:
    block: str
    name: str
    offset: str
    rwmode: str
    dwidth: int
    mask: Optional[str]
    description: str


@dataclass
class BlockInstance:
    inst_name: str
    block_def: str
    base_offset: str
    description: str


@dataclass
class DeviceInfo:
    hw_mod_name: str = ""
    hw_lif_name: str = ""
    hw_lif_vers: str = ""
    bus: str = ""
    endian: str = ""
    description: str = ""
    edge_version: str = ""


def parse_csv(filepath: str) -> tuple[DeviceInfo, list[Register], list[BlockInstance]]:
    """Parse the EDGE CSV file and extract registers and block instances."""
    registers: list[Register] = []
    instances: list[BlockInstance] = []
    info = DeviceInfo()

    current_section = None

    with open(filepath, "r") as f:
        lines = f.readlines()

    i = 0
    while i < len(lines):
        line = lines[i].strip()

        # Detect section headers
        if line.startswith("#Document Info"):
            current_section = "doc_info"
            i += 1  # Skip header definition line
            continue
        elif line.startswith("#LIF"):
            current_section = "lif"
            i += 1
            continue
        elif line.startswith("#Device Identification"):
            current_section = "device_id"
            i += 1
            continue
        elif line.startswith("#Resources"):
            current_section = "resources"
            i += 1
            continue
        elif line.startswith("#Block table"):
            current_section = "block"
            i += 1
            continue
        elif line.startswith("#Block instances"):
            current_section = "block_instances"
            i += 1
            continue
        elif line.startswith("#"):
            current_section = None
            i += 1
            continue

        # Skip empty lines
        if not line:
            i += 1
            continue

        # Parse based on current section
        if current_section == "doc_info":
            fields = [f.strip() for f in line.split(",")]
            if len(fields) >= 2 and fields[0] != "drv_type":
                info.edge_version = fields[1]

        elif current_section == "lif":
            fields = [f.strip() for f in line.split(",", 5)]
            if len(fields) >= 6 and fields[0] != "hw_mod_name":
                info.hw_mod_name = fields[0]
                info.hw_lif_name = fields[1]
                info.hw_lif_vers = fields[2]
                info.bus = fields[3]
                info.endian = fields[4]
                info.description = fields[5] if len(fields) > 5 else ""

        elif current_section == "block":
            # Parse register definitions
            # Split carefully: description may contain commas
            fields = [f.strip() for f in line.split(",", 9)]
            if len(fields) >= 9 and fields[0] != "block_def_name":
                mask = fields[7].strip() if fields[7].strip() else None
                desc = fields[9].strip() if len(fields) > 9 else ""

                registers.append(Register(
                    block=fields[0],
                    name=fields[2],
                    offset=fields[3],
                    rwmode=fields[4],
                    dwidth=int(fields[5]),
                    mask=mask,
                    description=desc,
                ))

        elif current_section == "block_instances":
            fields = [f.strip() for f in line.split(",", 4)]
            if len(fields) >= 4 and fields[0] != "block_inst_name":
                instances.append(BlockInstance(
                    inst_name=fields[0],
                    block_def=fields[1],
                    base_offset=fields[3],
                    description=fields[4] if len(fields) > 4 else "",
                ))

        i += 1

    return info, registers, instances


def to_upper_snake(name: str) -> str:
    """Convert register name to UPPER_SNAKE_CASE."""
    return name.strip().upper()


def generate_hpp(
        info: DeviceInfo,
        registers: list[Register],
        instances: list[BlockInstance],
        input_file: str,
        namespace: str,
) -> str:
    """Generate the C++ header file content."""

    # Build include guard from namespace
    guard = f"FPGA_{namespace.upper()}_REGISTERS_HPP"
    input_basename = os.path.basename(input_file)
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    lines: list[str] = []

    # Header
    lines.append(f"#ifndef {guard}")
    lines.append(f"#define {guard}")
    lines.append("")
    lines.append("// =============================================================================")
    lines.append(f"// AUTO-GENERATED from {input_basename} — DO NOT EDIT")
    lines.append(f"// Generated: {timestamp}")
    lines.append(f"// EDGE version: {info.edge_version}")
    lines.append(f"// Device: {info.hw_lif_name} ({info.description.strip()})")
    lines.append("// =============================================================================")
    lines.append("")
    lines.append("#include <cstdint>")
    lines.append("")
    lines.append(f"namespace fpga::regs::{namespace} {{")
    lines.append("")

    # Group registers by block
    blocks: dict[str, list[Register]] = {}
    for reg in registers:
        if reg.block not in blocks:
            blocks[reg.block] = []
        blocks[reg.block].append(reg)

    for block_name, block_regs in blocks.items():
        lines.append(f"// --- Block: {block_name} ---")
        lines.append("")

        # Separate full registers from fields
        full_regs = [r for r in block_regs if r.mask is None]
        field_regs = [r for r in block_regs if r.mask is not None]

        # Access mode comment helper
        def rwmode_str(mode: str) -> str:
            modes = {"r": "RO", "w": "WO", "rw": "RW"}
            return modes.get(mode.strip(), mode.strip().upper())

        # Register offsets
        if full_regs:
            # Calculate alignment for clean formatting
            max_name_len = max(len(to_upper_snake(r.name)) for r in full_regs)

            lines.append("// Register offsets")
            for reg in full_regs:
                name = to_upper_snake(reg.name)
                padding = " " * (max_name_len - len(name))
                width = f"uint{reg.dwidth}_t" if reg.dwidth <= 64 else f"{reg.dwidth}-bit"
                lines.append(
                    f"constexpr std::size_t {name}{padding} = {reg.offset};"
                    f"  // [{rwmode_str(reg.rwmode)}] {width} — {reg.description}"
                )
            lines.append("")

        # Field masks (grouped by parent register offset)
        if field_regs:
            # Group fields by their offset (parent register)
            parent_offsets: dict[str, list[Register]] = {}
            for reg in field_regs:
                if reg.offset not in parent_offsets:
                    parent_offsets[reg.offset] = []
                parent_offsets[reg.offset].append(reg)

            for offset, fields in parent_offsets.items():
                # Find parent register name
                parent = next(
                    (r for r in full_regs if r.offset == offset), None
                )
                parent_name = to_upper_snake(parent.name) if parent else f"REG_AT_{offset}"

                lines.append(f"// Field masks for {parent_name} (offset {offset})")
                lines.append(f"namespace {parent_name.lower()} {{")

                max_field_len = max(len(to_upper_snake(f.name)) for f in fields)

                for field in fields:
                    name = to_upper_snake(field.name)
                    padding = " " * (max_field_len - len(name))
                    # Pad mask to 8 hex digits for 32-bit or 16 for 64-bit
                    mask_val = field.mask.strip()
                    if field.dwidth <= 32:
                        mask_hex = f"0x{int(mask_val, 16):08x}"
                        type_str = "std::uint32_t"
                    else:
                        mask_hex = f"0x{int(mask_val, 16):016x}"
                        type_str = "std::uint64_t"

                    lines.append(
                        f"    constexpr {type_str} {name}{padding} = {mask_hex};"
                        f"  // [{rwmode_str(field.rwmode)}] {field.description}"
                    )

                lines.append(f"}} // namespace {parent_name.lower()}")
                lines.append("")

    # Block instances
    if instances:
        lines.append("// --- Block Instances ---")
        lines.append("")
        max_inst_len = max(len(to_upper_snake(i.inst_name)) for i in instances)
        for inst in instances:
            name = to_upper_snake(inst.inst_name)
            padding = " " * (max_inst_len - len(name))
            lines.append(
                f"constexpr std::size_t {name}_BASE{padding} = {inst.base_offset};"
                f"  // {inst.description}"
            )
        lines.append("")

    lines.append(f"}} // namespace fpga::regs::{namespace}")
    lines.append("")
    lines.append(f"#endif // {guard}")
    lines.append("")

    return "\n".join(lines)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    input_file = sys.argv[1]

    if len(sys.argv) >= 3:
        output_file = sys.argv[2]
    else:
        output_file = Path(input_file).stem + ".hpp"

    # Derive namespace from filename
    namespace = Path(input_file).stem.lower()
    namespace = re.sub(r"[^a-z0-9_]", "_", namespace)

    print(f"Parsing: {input_file}")
    info, registers, instances = parse_csv(input_file)
    print(f"  Found {len(registers)} register entries")
    print(f"  Found {len(instances)} block instances")

    hpp_content = generate_hpp(info, registers, instances, input_file, namespace)

    with open(output_file, "w") as f:
        f.write(hpp_content)

    print(f"Generated: {output_file}")


if __name__ == "__main__":
    main()