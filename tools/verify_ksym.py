
#!/usr/bin/env python3

import argparse
import struct
import sys

from gen_ksym import read_elf_symbols


ELF_MAGIC = b"\x7fELF"
ELFCLASS64 = 2
ELFDATA2LSB = 1

SHT_PROGBITS = 1


def read_ksym_section(filename):
    with open(filename, "rb") as f:
        data = f.read()

    if data[:4] != ELF_MAGIC:
        raise ValueError("File is not an ELF file")

    if len(data) < 64:
        raise ValueError("ELF header is incomplete")

    if data[4] != ELFCLASS64 or data[5] != ELFDATA2LSB:
        raise ValueError("Expected little-endian ELF64")

    header = struct.unpack_from("<16sHHIQQQIHHHHHH", data, 0)

    shoff = header[6]
    shentsize = header[11]
    shnum = header[12]
    shstrndx = header[13]

    if shentsize < 64 or shstrndx >= shnum:
        raise ValueError("Invalid ELF section headers")

    if shoff + shentsize * shnum > len(data):
        raise ValueError("Section headers exceed ELF file size")

    sections = []

    for i in range(shnum):
        offset = shoff + i * shentsize
        sections.append(
            struct.unpack_from("<IIQQQQIIQQ", data, offset)
        )

    # Read section names.
    str_section = sections[shstrndx]
    str_offset = str_section[4]
    str_size = str_section[5]
    str_data = data[str_offset:str_offset + str_size]

    if len(str_data) != str_size:
        raise ValueError("Invalid section name string table")

    # Find .ksym.
    for section in sections:
        name_idx = section[0]

        if name_idx >= len(str_data):
            continue

        end = str_data.find(b"\0", name_idx)

        if end < 0:
            continue

        name = str_data[name_idx:end]

        if name == b".ksym":
            if section[1] != SHT_PROGBITS:
                raise ValueError(".ksym is not PROGBITS")

            sec_addr = section[3]
            sec_offset = section[4]
            sec_size = section[5]

            sec_data = data[sec_offset:sec_offset + sec_size]

            if len(sec_data) != sec_size:
                raise ValueError("Invalid .ksym section size")

            return sec_addr, sec_data

    raise ValueError("ELF does not contain .ksym section")


def verify_symbols(filename):
    expected = read_elf_symbols(filename)
    sec_addr, sec_data = read_ksym_section(filename)

    # Each entry contains three 64-bit fields:
    # address, size, and name pointer.
    entry_size = 24
    expected_count = len(expected)
    count_offset = expected_count * entry_size

    if count_offset + 8 > len(sec_data):
        raise ValueError("Symbol table is truncated")

    actual_count = struct.unpack_from(
        "<Q", sec_data, count_offset
    )[0]

    if actual_count != expected_count:
        raise ValueError(
            f"Symbol count mismatch: "
            f"expected {expected_count}, found {actual_count}"
        )

    names_start = count_offset + 8

    for index, (expected_addr, expected_size, expected_name) in enumerate(expected):
        offset = index * entry_size

        addr, size, name_ptr = struct.unpack_from(
            "<QQQ", sec_data, offset
        )

        if addr != expected_addr:
            raise ValueError(
                f"Entry {index}: address mismatch "
                f"(expected 0x{expected_addr:x}, found 0x{addr:x})"
            )

        if size != expected_size:
            raise ValueError(
                f"Entry {index} ({expected_name}): size mismatch"
            )

        # Convert the absolute pointer into a .ksym section offset.
        name_offset = name_ptr - sec_addr

        if name_offset < names_start or name_offset >= len(sec_data):
            raise ValueError(
                f"Entry {index} ({expected_name}): invalid name pointer"
            )

        end = sec_data.find(b"\0", name_offset)

        if end < 0:
            raise ValueError(
                f"Entry {index} ({expected_name}): unterminated name"
            )

        actual_name = sec_data[name_offset:end].decode(
            "utf-8", errors="replace"
        )

        if actual_name != expected_name:
            raise ValueError(
                f"Entry {index}: name mismatch "
                f"(expected {expected_name}, found {actual_name})"
            )

    return actual_count


def main():
    parser = argparse.ArgumentParser(
        description="Verify the embedded kernel symbol table"
    )

    parser.add_argument("elf", help="Final kernel ELF file")
    args = parser.parse_args()

    try:
        count = verify_symbols(args.elf)

        print(
            f"Symbol verifier: PASS "
            f"({count} symbols checked in {args.elf})"
        )

    except (OSError, ValueError, struct.error, IndexError) as error:
        print(f"Symbol verifier: FAIL: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
