
#!/usr/bin/env python3

import argparse
import struct
import sys


ELF_MAGIC = b"\x7fELF"
ELFCLASS64 = 2
ELFDATA2LSB = 1

SHT_SYMTAB = 2
STT_FUNC = 2


def read_elf_symbols(filename):
    with open(filename, "rb") as f:
        data = f.read()

    if data[:4] != ELF_MAGIC:
        raise ValueError("File is not an ELF file")

    if data[4] != ELFCLASS64:
        raise ValueError("Only ELF64 is supported")

    if data[5] != ELFDATA2LSB:
        raise ValueError("Only little-endian ELF is supported")

    header = struct.unpack_from("<16sHHIQQQIHHHHHH", data, 0)

    shoff = header[6]
    shentsize = header[11]
    shnum = header[12]

    sections = []

    for i in range(shnum):
        offset = shoff + i * shentsize
        section = struct.unpack_from("<IIQQQQIIQQ", data, offset)
        sections.append(section)

    symbols = []

    for section in sections:
        if section[1] != SHT_SYMTAB:
            continue

        sh_offset = section[4]
        sh_size = section[5]
        sh_link = section[6]
        sh_entsize = section[9]

        if sh_entsize == 0:
            continue

        str_section = sections[sh_link]
        str_offset = str_section[4]
        str_size = str_section[5]
        str_data = data[str_offset:str_offset + str_size]

        count = sh_size // sh_entsize

        for i in range(count):
            offset = sh_offset + i * sh_entsize

            name_idx, info, other, shndx, value, size = (
                struct.unpack_from("<IBBHQQ", data, offset)
            )

            if (info & 0x0F) != STT_FUNC:
                continue

            if shndx == 0 or size == 0:
                continue

            if name_idx >= len(str_data):
                continue

            end = str_data.find(b"\0", name_idx)

            if end == -1:
                continue

            name = str_data[name_idx:end].decode(
                "utf-8", errors="replace"
            )

            if name:
                symbols.append((value, size, name))

    symbols.sort(key=lambda item: item[0])

    # Remove duplicate entries with the same starting address.
    unique = []
    seen = set()

    for symbol in symbols:
        if symbol[0] not in seen:
            unique.append(symbol)
            seen.add(symbol[0])

    return unique


def emit_assembly(symbols, output):
    with open(output, "w", encoding="utf-8") as f:
        f.write('.section .ksym,"a",@progbits\n')
        f.write(".balign 8\n")
        f.write(".globl ksym_table\n")
        f.write(".type ksym_table, @object\n")
        f.write("ksym_table:\n")

        for index, (address, size, name) in enumerate(symbols):
            f.write(f"  .quad 0x{address:x}\n")
            f.write(f"  .quad {size}\n")
            f.write(f"  .quad .Lksym_name_{index}\n")

        f.write(".size ksym_table, .-ksym_table\n")

        f.write(".globl ksym_count\n")
        f.write(".type ksym_count, @object\n")
        f.write("ksym_count:\n")
        f.write(f"  .quad {len(symbols)}\n")
        f.write(".size ksym_count, 8\n")

        for index, (_, _, name) in enumerate(symbols):
            encoded = name.encode("utf-8") + b"\0"

            f.write(f".Lksym_name_{index}:\n")

            for byte in encoded:
                f.write(f"  .byte {byte}\n")


def main():
    parser = argparse.ArgumentParser(
        description="Generate a kernel symbol table from an ELF64 file"
    )

    parser.add_argument("elf", help="Input ELF file")
    parser.add_argument("-o", "--output", help="Output assembly file")

    args = parser.parse_args()

    try:
        symbols = read_elf_symbols(args.elf)

        if args.output:
            emit_assembly(symbols, args.output)
            print(f"Generated {len(symbols)} symbols in {args.output}")
        else:
            print(f"Found {len(symbols)} function symbols")

            for address, size, name in symbols:
                print(f"0x{address:016x} {size:5d} {name}")

    except (OSError, ValueError, struct.error, IndexError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
