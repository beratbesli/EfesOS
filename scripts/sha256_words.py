"""Print SHA-256 digest words for the EfesOS stage-2 verifier."""

import hashlib
import struct
import sys


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: sha256_words.py FILE", file=sys.stderr)
        return 2
    with open(sys.argv[1], "rb") as stream:
        digest = hashlib.sha256(stream.read()).digest()
    words = struct.unpack(">8I", digest)
    print(" ".join(f"0x{word:08X}" for word in words))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
