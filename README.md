# BeerOS

BeerOS is an experimental 32-bit x86 hobby operating system written in C and NASM assembly. It boots from a BIOS floppy image and runs in QEMU.

## Status

BeerOS is a learning project, not a production operating system. It has no user-mode isolation, executable permission enforcement, disk filesystem, authentication, secure boot, or persistent storage. Do not use it with sensitive data or as a security boundary.

## Features

- BIOS stage-1 bootloader with a 1.44 MiB floppy image
- 32-bit protected mode, GDT, IDT, PIC, PIT and hardware keyboard input
- Identity-mapped paging for the first 4 MiB and a physical-memory bitmap allocator
- VGA text/graphics output with scrolling
- English (US) and Turkish Q keyboard modes
- Interactive shell, RAM filesystem, scheduler demo, Snake and slot games

## Requirements

- NASM
- `i686-elf-gcc`, `i686-elf-ld`, `i686-elf-objcopy`
- QEMU (`qemu-system-i386`)

Use a toolchain obtained from a trusted source and verify its checksum before installing it. BeerOS does not download or vendor compiler binaries.

## Build and run on Windows

From PowerShell in the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Run
```

The script builds `build\beeros.img`, validates its size and starts QEMU. Without `-Run`, it only builds the image.

## Shell commands

| Command | Description |
| --- | --- |
| `help`, `clear`, `about`, `mem` | Basic shell information |
| `uptime`, `ps`, `demo`, `counter` | Kernel and scheduler status |
| `echo`, `history`, `color` | Shell utilities |
| `ls`, `cat README`, `cat MOTD`, `cat BEER` | RAM filesystem |
| `snake`, `slot` | Mini games |
| `en`, `tr` | Switch language and keyboard layout |
| `reboot`, `shutdown` | Control the QEMU guest |

Snake uses `W`, `A`, `S`, `D` to move and `Q` to exit. Slot uses Space to spin and `Q` to exit.

## Repository layout

```text
boot/       BIOS boot sector
cpu/        IDT, PIC, PIT and interrupt stubs
drivers/    VGA and keyboard drivers
fs/         In-memory filesystem
games/      Snake and slot game logic
include/    Shared headers
kernel/     Entry point, GDT, splash and linker script
memory/     Physical memory manager and paging
process/    Scheduler and demo tasks
scripts/    Windows build helper
shell/      Command shell
```

## Security notes

- CPU exceptions are caught and halt the guest instead of escalating to an unhandled triple fault.
- Shell input and command history use fixed, bounded buffers.
- The project intentionally runs all code in ring 0 and maps kernel memory as writable. This is appropriate only for a hobby OS and is not a security model.
- See [SECURITY.md](SECURITY.md) for reporting guidance.

## License

BeerOS is released under the [MIT License](LICENSE).
