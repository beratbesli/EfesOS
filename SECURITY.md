# Security Policy

## Supported versions

Only the latest commit on `main` is supported.

## Reporting a vulnerability

Do not publish exploit details in a public issue first. Use GitHub's private security advisory feature for this repository and include a minimal reproduction, affected commit and expected impact.

EfesOS is an educational ring-0 kernel without a user/kernel security boundary. Reports about memory corruption, arbitrary control flow, unsafe boot-image handling or toolchain supply-chain risks are still welcome.

## Current security boundary

- Kernel text/rodata are write-protected after paging is enabled; the null page is unmapped and the heap has guard pages and canaries.
- Ring-3 tasks use supervisor-inaccessible code/stack pages, a dedicated TSS transition stack and a restricted `int 0x80` ABI. User exceptions terminate the task instead of panicking the kernel.
- ELF32 validation rejects malformed ranges, integer-overflowable sizes, unsupported machines and writable/executable segments before any future loader uses them.
- ATA access is bounded and timeout-controlled. FAT16 support is read-only; no shell command can write arbitrary disk sectors.

This remains a learning kernel. It has no authentication, secure boot, signed modules, ASLR, SMP isolation, complete ELF loading, user-pointer validation for data-bearing syscalls, or a fully validated persistent filesystem. Do not treat it as a production security boundary until those items are implemented and audited.

Every pull request should pass the LLVM/GCC build, deterministic FAT host test and QEMU ring-3 fault-isolation smoke test defined in `.github/workflows/ci.yml`.
