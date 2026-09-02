"""Boot a one-byte-corrupted image and require stage-2 to reject it."""
import pathlib
import shutil
import subprocess
import sys
import tempfile
import os


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[1]
    image = root / "build" / "efesos.img"
    if not image.is_file():
        print(f"disk image not found: {image}", file=sys.stderr)
        return 2
    qemu = shutil.which("qemu-system-i386")
    if qemu is None:
        program_files = os.environ.get("ProgramFiles", "")
        for candidate in (
            pathlib.Path(program_files) / "qemu" / "qemu-system-i386.exe",
            pathlib.Path(program_files) / "QEMU" / "qemu-system-i386.exe",
        ):
            if candidate.is_file():
                qemu = str(candidate)
                break
    if qemu is None:
        print("qemu-system-i386 not found", file=sys.stderr)
        return 2

    data = bytearray(image.read_bytes())
    kernel_offset = (1 + 12) * 512
    if kernel_offset >= len(data):
        print("kernel offset is outside the disk image", file=sys.stderr)
        return 1
    data[kernel_offset] ^= 0x01

    with tempfile.TemporaryDirectory(prefix="efesos-sha-") as directory:
        directory_path = pathlib.Path(directory)
        mutated = directory_path / "negative.img"
        serial = directory_path / "serial.log"
        mutated.write_bytes(data)
        command = [
            qemu,
            "-display", "none", "-monitor", "none",
            "-serial", f"file:{serial}",
            "-no-reboot", "-no-shutdown", "-m", "32",
            "-drive", f"file={mutated},format=raw,if=floppy", "-boot", "a",
        ]
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        output = serial.read_text(errors="replace") if serial.exists() else ""

    if "!" not in output or "EfesOS: kernel entry reached." in output:
        print("corrupted kernel image was not rejected before kernel entry", file=sys.stderr)
        return 1
    print("SHA-256 boot negative test passed: corrupted kernel was rejected before kernel entry.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
