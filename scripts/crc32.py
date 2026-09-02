#!/usr/bin/env python3
"""Print the CRC-32 used by the EfesOS stage-2 kernel integrity check."""

import sys
import zlib


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: crc32.py FILE", file=sys.stderr)
        return 2
    with open(sys.argv[1], "rb") as stream:
        checksum = zlib.crc32(stream.read()) & 0xFFFFFFFF
    print(f"0x{checksum:08X}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
