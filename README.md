# EfesOS

EfesOS is an experimental 32-bit x86 operating system written in C and NASM assembly. It boots from a BIOS floppy image and runs in QEMU.

[Türkçe README](README.tr.md)

## Status

EfesOS is a learning project, not a production operating system. It now has a small ring-3 demo boundary, validated ELF segment loading, read-only kernel text pages, and a read-only FAT16 probe, but it still lacks authentication, secure boot, signed binaries, and a production-grade persistent filesystem. Do not use it with sensitive data or as a security boundary.

## Features

- Retried two-stage BIOS bootloader with A20 verification and a 1.44 MiB floppy image
- BIOS E820 memory map handoff, deterministic `.bss` initialization, strict metadata validation and reserved-over-usable overlap normalization
- Early CPUID capability probe reports PAE/NX/TSC/RDRAND/MSR/APIC/x2APIC support; PAE paging and hardware NX are enabled when supported, with a legacy fallback
- 32-bit protected mode, GDT, vector-aware IDT, validated xAPIC/IOAPIC routing for IRQ0/IRQ1/IRQ14, dedicated AHCI MSI vector 51, dual-8259 PIC fallback, PIT and buffered hardware keyboard input
- Stable CMOS RTC wall-clock reads with UIP/format/calendar validation and a `date` shell command
- Bounded ACPI RSDP/RSDT/XSDT discovery with checksum validation plus guarded MADT topology/interrupt-override and HPET table parsing
- Validated HPET monotonic clock with uncached MMIO, nanosecond conversion, 32-bit counter-wrap maintenance and an automatic PIT fallback
- Preemptive kernel-thread scheduler with guarded per-task stacks, an HPET-calibrated periodic local APIC timer and an automatic PIT fallback
- Bounded priority time slices with explicit voluntary-yield handling
- Deferred event loop; shell and games never run inside hardware IRQ handlers
- PCI configuration-space enumeration with read-only type-0 BAR decoding and a bounded `pci` diagnostic command
- PCI BAR records pass an in-kernel alignment/type self-test before device drivers consume them
- Timeout-bounded ATA primary-master I/O with IRQ14 completion, validated bus-master DMA reads in 4 KiB bounce-buffer chunks, automatic PIO fallback, serialized requests and explicit disk absence reporting
- Bounded read-only AHCI path for Q35/ICH9 SATA: ordered failover across usable controllers, up to eight validated disks on the selected controller, safely quiesced port switching over shared DMA pages, BIOS ownership handoff, cache-disabled BAR5 mapping, transactional single-message MSI completion with generation tracking and polling fallback, serialized slot-0 IDENTIFY/READ DMA commands, one COMRESET retry followed by one controller-wide HBA-reset retry with per-device generation revalidation, and fail-closed MSI/bus-master revocation
- Driver-independent 512-byte block-device layer validates capacity, transfer bounds and optional write capability before dispatch; VFS receives a read-only ATA or AHCI view
- ATA raw writes remain disabled by default; only a validated journal window can be transactionally enabled
- Read-only FAT16 VFS mount with bounded 8.3 root/subdirectory file reads (`diskls`, `diskcat NAME`, `diskcat DIR/NAME`); validated ELF launch from disk (`run NAME`)
- When a validated journal region exists outside the FAT volume, shell `write`/`rm` operations are committed transactionally to persistent RAMFS; `pformat` explicitly formats an entirely empty journal tail
- Bounded ELF32 segment loader with BSS initialization, W^X checks and page-permission finalization
- Software execute metadata for ELF code pages with EIP checks at scheduler/syscall boundaries (not a full replacement for hardware NX in non-PAE mode)
- Bounded per-boot user-stack and ET_DYN load-layout diversification; CPU RDRAND is mixed into the seed when available, with a non-cryptographic fallback (not full ASLR)
- Relocation-free position-independent `ET_DYN` images receive a bounded randomized load bias; legacy `ET_EXEC` images remain fixed
- Bounded user-buffer validation for the data-carrying serial syscall, including overflow and permission checks
- Faulted demo processes release their user ELF pages and stack frames before scheduling continues
- Up to eight bounded user processes get private page directories; the scheduler switches address spaces with CR3 and reuses slots after faults
- Kernel-only bounded `user_process_spawn` API loads validated ELF images into owned address spaces with automatic stack/cleanup ownership
- User stacks include an unmapped guard page so downward stack overflow faults before reaching adjacent mappings
- New user processes use one of sixteen bounded stack regions, reducing assumptions about a single fixed user-stack address
- IPC syscalls (`IPC_SEND`, `IPC_RECEIVE`, `IPC_SEND_TO`, `IPC_RECEIVE_WAIT`, `EXIT`) with 16-message/64-byte bounds, generation-PID routing, scheduler wakeups, and validated user copies
- Generation-based `GET_PID` syscall so reused task slots do not retain stale identities
- Stage-2 SHA-256 kernel integrity check before protected-mode handoff (integrity, not authenticity)
- Bounded user `EXIT` lifecycle with resource reclamation and scheduler slot reuse
- E820-backed physical-memory allocation across the 32-bit address space
- Null-page protection, read-only kernel code/data, dynamic page mapping and a guarded kernel heap
- VGA text/graphics output with scrolling
- English (US) and Turkish Q keyboard modes
- Interactive shell, bounded writable RAM filesystem, scheduler demo, Snake and slot games

## Requirements

- NASM
- Either `i686-elf-gcc`, `i686-elf-ld`, and `i686-elf-objcopy`, or LLVM (`clang`, `ld.lld`, and `llvm-objcopy`)
- QEMU (`qemu-system-i386`)
- Python 3 (Linux `make` build’inde kernel SHA-256 kelimelerini üretmek için)

The Windows build script prefers the GNU cross-toolchain when it is available and otherwise uses Clang with the `i686-none-elf` target. Use tools obtained from trusted sources and verify their checksums before installing them. EfesOS does not download or vendor compiler binaries.

For supply-chain safety, the build script does not search the repository's `tools` directory by default. If you intentionally place verified binaries there, opt in with `-AllowLocalTools` and verify their hashes first.

To run the host-side integrity regression (including a one-byte corruption negative case), run `make sha256-self-test` after building. `make sha256-boot-negative-test` additionally boots a corrupted image and checks that stage-2 rejects it before kernel entry.

For release distribution, sign the complete image with a trusted external RSA-3072 key and verify it before publishing or booting:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\sign-release.ps1 -PrivateKeyPath .\release-key.pem -SignaturePath .\build\efesos.img.sig
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\verify-release-signature.ps1 -PublicKeyPath .\release-key.pub.pem -SignaturePath .\build\efesos.img.sig
```

This authenticates a release artifact only when the public key is trusted out-of-band; the BIOS floppy loader still does not provide hardware secure boot.

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

Run the standalone block-device boundary and capability test when changing storage drivers:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\block-device-self-test.ps1
```

Run the ATA IRQ completion state-machine test when changing ATA or PIC code:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ata-irq-self-test.ps1
```

Run the ATA DMA controller, PRDT, transfer-mode and completion contract test:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ata-dma-self-test.ps1
```

Validate AHCI PCI class (`01/06/01`) and BAR5 MMIO layout handling:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\pci-ahci-self-test.ps1
```

Validate bounded PCI MSI capability parsing, transactional register rollback and AHCI interrupt-generation tracking:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\pci-msi-self-test.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-irq-state-self-test.ps1
```

Run the RTC BCD/binary, 12/24-hour and Gregorian calendar conversion test:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\rtc-self-test.ps1
```

Run the bounded ACPI table parser and HPET time-conversion tests:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\acpi-self-test.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\madt-self-test.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\hpet-self-test.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1 -RequireHpet
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1 -DisableHpet
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1 -DisableAcpi
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1 -DisableApic
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1 -Q35 -RequireHpet
```

The HPET profile verifies both the live MMIO counter and the calibrated local APIC scheduler timer. The HPET-disabled profile verifies PIT delivery through the IOAPIC; the ACPI-disabled and APIC-disabled profiles verify the masked-PIC fallback. The Q35 profile verifies the ACPI/MADT/IOAPIC/HPET and scheduler paths on the newer ICH9 platform model, including bounded AHCI class and BAR5 discovery. When a disk image is supplied, the Q35 profile also verifies read-only AHCI IDENTIFY, MSI-completed DMA reads with zero polling fallbacks and FAT mounting. `-Q35 -DisableApic` verifies the same reads through the polling fallback without enabling MSI.

Run the deterministic boot metadata, E820, ELF and FAT property-fuzz suite after changing any boot or parser boundary:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\parser-fuzz-self-test.ps1
```

Run the standalone FAT parser test when changing filesystem code:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\fat-self-test.ps1
```

Run the bounded RAM filesystem test when changing RAMFS code:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ramfs-self-test.ps1
```

For journal record format changes, run the CRC/commit validation test:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\journal-self-test.ps1
```

Exercise persistent RAMFS journal append, reboot replay and idempotent removal with an in-memory ATA backend:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\persistent-self-test.ps1
```

Run the standalone ELF validation test when changing process loading code:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\elf-self-test.ps1
```

Run the E820 and VGA-font validation test when changing boot metadata:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\boot-info-self-test.ps1
```

Exercise the complete QEMU ATA/FAT read path with the deterministic fixture:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\create-test-disk.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1 -DiskImage .\build\test-disk.img
```

Run the same FAT fixture through Q35/ICH9 AHCI:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1 -SkipBuild -Q35 -RequireHpet -DiskImage .\build\test-disk.img
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1 -SkipBuild -Q35 -DisableApic -DiskImage .\build\test-disk.img
```

Exercise recoverable and persistent AHCI read errors through QEMU `blkdebug`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-recovery-self-test.ps1 -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-recovery-self-test.ps1 -SkipBuild -DisableHpet
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-recovery-self-test.ps1 -SkipBuild -HbaEscalation
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-recovery-self-test.ps1 -SkipBuild -HbaEscalation -DisableHpet
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-recovery-self-test.ps1 -SkipBuild -HbaEscalation -DisableApic
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-recovery-self-test.ps1 -SkipBuild -PersistentFailure
```

Exercise bounded controller selection with an empty first controller, APIC-less
polling, and an all-empty exhaustion profile:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-controller-failover-self-test.ps1 -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-controller-failover-self-test.ps1 -SkipBuild -DisableApic
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-controller-failover-self-test.ps1 -SkipBuild -AllEmpty
```

Exercise two SATA disks on separate ports of one Q35/ICH9 controller, both with
MSI completion and with the APIC-disabled polling fallback:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-multi-device-self-test.ps1 -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\ahci-multi-device-self-test.ps1 -SkipBuild -DisableApic
```

The fixture also replays one journal record from a region outside the FAT volume; disk writes remain protected.

Exercise the interactive shell-to-ring-3 disk ELF path as well:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-self-test.ps1
```

To additionally exercise an actual journal-window write on the QEMU test disk:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-self-test.ps1 -TestPersistentWrite
```

To exercise the explicit format path on a blank journal tail as well:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\run-self-test.ps1 -TestPersistentFormat
```

On a deliberately empty, non-overlapping disk tail, type `pformat` in the shell
to initialize persistent RAMFS. Formatting refuses non-empty disk tails,
already formatted regions, and a RAMFS changed beyond its built-in defaults.

Check that two consecutive image builds are byte-for-byte reproducible:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\reproducible-build-self-test.ps1
```

## Shell commands

| Command | Description |
| --- | --- |
| `help`, `clear`, `about`, `mem`, `heap`, `input` | Basic kernel and queue information |
| `uptime`, `date`, `ps`, `demo`, `pci`, `disk`, `diskls`, `diskcat NAME`, `run NAME`, `counter` | Clock, kernel, PCI, disk, scheduler and validated user-process launch |
| `echo`, `history`, `color` | Shell utilities |
| `ls`, `cat README`, `cat MOTD`, `cat EFES`, `write NAME CONTENT`, `rm NAME` | Bounded RAM filesystem |
| `snake`, `slot` | Mini games |
| `en`, `tr` | Switch language and keyboard layout |
| `reboot`, `shutdown` | Control the QEMU guest |

Snake uses `W`, `A`, `S`, `D` to move and `Q` to exit. Slot uses Space to spin and `Q` to exit.

## Repository layout

```text
boot/       BIOS stage-1 and stage-2 loaders
cpu/        IDT, xAPIC/IOAPIC, PIC/PIT and interrupt stubs
drivers/    VGA, keyboard, RTC, ACPI/HPET, PCI, ATA/AHCI and generic block-device drivers
fs/         In-memory filesystem
games/      Snake and slot game logic
include/    Shared headers
kernel/     Entry point, GDT, splash and linker script
memory/     Physical/virtual memory managers and guarded kernel heap
process/    Scheduler and demo tasks
scripts/    Windows build, QEMU smoke-test and disk ELF launch helpers
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
