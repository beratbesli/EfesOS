"""Check the build digest and prove that a one-byte image mutation is detected."""
import hashlib
import pathlib
import sys


def main() -> int:
    path = pathlib.Path(sys.argv[1]) if len(sys.argv) == 2 else pathlib.Path("build/kernel.bin")
    if len(sys.argv) > 2:
        print(f"usage: {pathlib.Path(sys.argv[0]).name} [KERNEL]", file=sys.stderr)
        return 2
    data = path.read_bytes()
    if not data:
        print("kernel image is empty", file=sys.stderr)
        return 1
    digest = hashlib.sha256(data).digest()
    mutated = bytearray(data)
    mutated[0] ^= 0x01
    if hashlib.sha256(mutated).digest() == digest:
        print("one-byte corruption was not detected", file=sys.stderr)
        return 1
    print("SHA-256 self-test passed: digest matches and one-byte corruption is detected.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
