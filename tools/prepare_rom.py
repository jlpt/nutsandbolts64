#!/usr/bin/env python3
"""Normalise a Banjo-Kazooie (USA v1.0) ROM to big-endian .z64 and verify it.

    prepare_rom.py <your rom (.z64/.n64/.v64)> <out.z64>

Accepts any byte order, and Windows paths (C:\\...) when run under WSL.
Only Banjo-Kazooie USA 1.0 is supported because that's what the decomp
builds; other ROMs are identified and rejected with a hint.
"""
import hashlib
import os
import re
import sys

SHA1_US10 = "1fe1632098865f639e22c11b9a81ee8f29c75d7a"
KNOWN = {
    "ded6ee166e740ad1bc810fd678a84b48e245ab80": "Banjo-Kazooie USA Rev 1 (v1.1)",
    "bb359a75941df74bf7290212c89fbc6e2c5601fe": "Banjo-Kazooie Europe (PAL)",
    "90726d7e7cd5bf6cdfd38f45c9acbf4d45bd9fd8": "Banjo-Kazooie Japan",
}
HINT = ("This mod needs Banjo-Kazooie USA v1.0 (the first N64 game). "
        "Expected sha1 %s for the .z64." % SHA1_US10)


def windows_to_wsl(path):
    m = re.match(r"^([A-Za-z]):[\\/](.*)$", path)
    if not m or os.path.exists(path):
        return path
    return "/mnt/%s/%s" % (m.group(1).lower(), m.group(2).replace("\\", "/"))


def to_z64(data):
    sig = data[:4]
    if sig == b"\x80\x37\x12\x40":
        return data
    out = bytearray(len(data))
    if sig == b"\x37\x80\x40\x12":       # .v64 (byte-swapped)
        out[0::2] = data[1::2]
        out[1::2] = data[0::2]
    elif sig == b"\x40\x12\x37\x80":     # .n64 (little-endian)
        out[0::4] = data[3::4]
        out[1::4] = data[2::4]
        out[2::4] = data[1::4]
        out[3::4] = data[0::4]
    else:
        sys.exit("That file isn't an N64 ROM (unknown header %s). %s" % (sig.hex(), HINT))
    return bytes(out)


def main():
    if len(sys.argv) != 3 or not sys.argv[1] or sys.argv[1] == "baserom.us.v10.z64" and not os.path.exists(sys.argv[1]):
        sys.exit('No ROM given. Run: make BASEROM="/path/to/Banjo-Kazooie (USA).z64"\n' + HINT)
    src = windows_to_wsl(sys.argv[1])
    dst = sys.argv[2]
    if not os.path.isfile(src):
        sys.exit("ROM not found: %s\nTip: put the path in quotes, and under WSL use /mnt/c/... "
                 "(e.g. \"/mnt/c/Users/you/Downloads/Banjo-Kazooie (USA).z64\")." % src)

    rom = to_z64(open(src, "rb").read())
    sha = hashlib.sha1(rom).hexdigest()
    if sha == SHA1_US10:
        open(dst, "wb").write(rom)
        print("ok: Banjo-Kazooie USA v1.0 -> %s" % dst)
        return
    title = rom[0x20:0x34].decode("ascii", "replace").strip()
    if sha in KNOWN:
        sys.exit("This is %s. %s" % (KNOWN[sha], HINT))
    if "TOOIE" in title.upper():
        sys.exit("This is Banjo-Tooie, not Banjo-Kazooie. %s" % HINT)
    sys.exit("Unrecognised ROM: internal title \"%s\", sha1 %s. %s" % (title, sha, HINT))


if __name__ == "__main__":
    main()
