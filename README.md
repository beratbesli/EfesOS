# EfesOS

EfesOS is an experimental 32-bit x86 operating system written in C and NASM assembly. It boots from a BIOS floppy image and runs in QEMU.

[Türkçe README](README.tr.md)

## Status

EfesOS is a learning project, not a production operating system. It now has a small ring-3 demo boundary, validated ELF segment loading, read-only kernel text pages, and a read-only FAT16 probe, but it still lacks authentication, secure boot, signed binaries, and a production-grade persistent filesystem. Do not use it with sensitive data or as a security boundary.

## Features

- Retried two-stage BIOS bootloader with A20 verification and a 1.44 MiB floppy image
- BIOS E820 memory map handoff and deterministic `.bss` initialization
- 32-bit protected mode, GDT, vector-aware IDT, PIC, PIT and buffered hardware keyboard input
- Preemptive kernel-thread scheduler with guarded per-task stacks and timer-driven context switching
- Deferred event loop; shell and games never run inside hardware IRQ handlers
- PCI configuration-space enumeration with a bounded `pci` diagnostic command
- Timeout-bounded ATA PIO primary-master block I/O with explicit disk absence reporting
- Read-only FAT16 VFS mount with bounded 8.3 directory and file reads (`diskls`, `diskcat`)
- Bounded ELF32 segment loader with BSS initialization, W^X checks and page-permission finalization
- Bounded user-buffer validation for the data-carrying serial syscall, including overflow and permission checks
- E820-backed physical-memory allocation across the 32-bit address space
- Null-page protection, read-only kernel code/data, dynamic page mapping and a guarded kernel heap
- VGA text/graphics output with scrolling
- English (US) and Turkish Q keyboard modes
- Interactive shell, bounded writable RAM filesystem, scheduler demo, Snake and slot games

## Requirements

- NASM
- Either `i686-elf-gcc`, `i686-elf-ld`, and `i686-elf-objcopy`, or LLVM (`clang`, `ld.lld`, and `llvm-objcopy`)
- QEMU (`qemu-system-i386`)

The Windows build script prefers the GNU cross-toolchain when it is available and otherwise uses Clang with the `i686-none-elf` target. Use tools obtained from trusted sources and verify their checksums before installing them. EfesOS does not download or vendor compiler binaries.

## Build and run on Windows

From PowerShell in the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Run
```

The script builds `build\efesos.img`, validates its size and starts QEMU. Without `-Run`, it only builds the image.

Run the headless boot smoke test after kernel changes:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1
```

The test boots the generated image in QEMU and requires the expected kernel milestone on COM1 before it passes.

Run the standalone FAT parser test when changing filesystem code:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\fat-self-test.ps1
```

Exercise the complete QEMU ATA/FAT read path with the deterministic fixture:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\create-test-disk.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1 -DiskImage .\build\test-disk.img
```

## Shell commands

| Command | Description |
| --- | --- |
| `help`, `clear`, `about`, `mem`, `heap`, `input` | Basic kernel and queue information |
| `uptime`, `ps`, `demo`, `pci`, `disk`, `diskls`, `diskcat NAME`, `counter` | Kernel, PCI, disk and scheduler status |
| `echo`, `history`, `color` | Shell utilities |
| `ls`, `cat README`, `cat MOTD`, `cat EFES`, `write NAME CONTENT`, `rm NAME` | Bounded RAM filesystem |
| `snake`, `slot` | Mini games |
| `en`, `tr` | Switch language and keyboard layout |
| `reboot`, `shutdown` | Control the QEMU guest |

Snake uses `W`, `A`, `S`, `D` to move and `Q` to exit. Slot uses Space to spin and `Q` to exit.

## Repository layout

```text
boot/       BIOS stage-1 and stage-2 loaders
cpu/        IDT, PIC, PIT and interrupt stubs
drivers/    VGA and keyboard drivers
fs/         In-memory filesystem
games/      Snake and slot game logic
include/    Shared headers
kernel/     Entry point, GDT, splash and linker script
memory/     Physical/virtual memory managers and guarded kernel heap
process/    Scheduler and demo tasks
scripts/    Windows build and QEMU smoke-test helpers
shell/      Command shell
```

## Security notes

- CPU exceptions are decoded; faults in the demo ring-3 task are isolated, while unrecoverable kernel faults halt the guest.
- Shell input and command history use fixed, bounded buffers.
- Keyboard IRQ input uses a bounded single-producer/single-consumer queue and reports dropped input.
- A deliberately small ring-3 task uses a TSS transition stack, user-only pages, a restricted syscall ABI, and fault termination. This is a demonstration boundary, not a complete process model.
- See [SECURITY.md](SECURITY.md) for reporting guidance.

## License

EfesOS is released under the [MIT License](LICENSE).
