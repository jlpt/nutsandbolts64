#!/usr/bin/env python3
"""Append the mod image to the rebuilt Banjo-Kazooie ROM.

    mkrom.py <banjo.us.v10.z64> <nb.bin> <out.z64>

The mod lives at ROM offset 0x01000000 (just past the original 16 MB) and is
loaded into Expansion Pak RAM by the hook in core1. The IPL3 checksum only
covers 0x1000-0x101000, so appending doesn't change it.
"""
import struct
import sys

NB_ROM_OFFSET = 0x01000000
NB_MAGIC = 0x4E423634


def main():
    base_path, mod_path, out_path = sys.argv[1:4]
    rom = bytearray(open(base_path, "rb").read())
    mod = open(mod_path, "rb").read()
    if len(rom) != NB_ROM_OFFSET:
        sys.exit("expected a 16 MB base ROM, got %d bytes" % len(rom))
    magic, version, load_size = struct.unpack(">III", mod[:12])
    if magic != NB_MAGIC:
        sys.exit("mod image has a bad header (magic %08X)" % magic)
    if load_size > len(mod) + 8:
        sys.exit("mod header load_size %X exceeds image size %X" % (load_size, len(mod)))
    mod = mod + b"\0" * (-len(mod) % 16)
    rom += mod
    size = (len(rom) + 0x3FFFFF) & ~0x3FFFFF  # round up to 4 MB
    rom += b"\xFF" * (size - len(rom))
    open(out_path, "wb").write(rom)
    print("wrote %s: %.1f MB ROM, mod image %d KB (version %d)" % (out_path, size / 1048576.0, len(mod) // 1024, version))


if __name__ == "__main__":
    main()
