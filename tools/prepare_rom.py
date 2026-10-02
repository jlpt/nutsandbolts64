#!/usr/bin/env python3
"""Normalise a Banjo-Kazooie (USA v1.0) ROM to big-endian .z64 and verify it.

    prepare_rom.py <your rom (.z64/.n64/.v64)> <out.z64>

Accepts any byte order. Only the USA 1.0 release is supported because that's
what the decomp builds; Rev 1 (v1.1) is detected and rejected with a hint.
"""
import hashlib
import sys

SHA1_US10 = "1fe1632098865f639e22c11b9a81ee8f29c75d7a"
SHA1_US11 = "ded6ee166e740ad1bc810fd678a84b48e245ab80"


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
        sys.exit("not an N64 ROM (unknown header %s)" % sig.hex())
    return bytes(out)


def main():
    src, dst = sys.argv[1:3]
    rom = to_z64(open(src, "rb").read())
    sha = hashlib.sha1(rom).hexdigest()
    if sha == SHA1_US11:
        sys.exit("This is Banjo-Kazooie USA Rev 1 (v1.1). The decomp only builds USA v1.0 - "
                 "please use a v1.0 dump (sha1 %s)." % SHA1_US10)
    if sha != SHA1_US10:
        sys.exit("Unrecognised ROM (sha1 %s). Expected Banjo-Kazooie USA v1.0 (sha1 %s)." % (sha, SHA1_US10))
    open(dst, "wb").write(rom)
    print("ok: Banjo-Kazooie USA v1.0 -> %s" % dst)


if __name__ == "__main__":
    main()
