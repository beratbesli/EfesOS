# Security Policy

## Supported versions

Only the latest commit on `main` is supported.

## Reporting a vulnerability

Do not publish exploit details in a public issue first. Use GitHub's private security advisory feature for this repository and include a minimal reproduction, affected commit and expected impact.

EfesOS is an educational kernel with a deliberately small ring-3 demonstration boundary. Reports about memory corruption, arbitrary control flow, unsafe boot-image handling or toolchain supply-chain risks are still welcome.

## Current security boundary

- Kernel text/rodata are write-protected after paging is enabled; the null page is unmapped and the heap has guard pages and canaries.
- E820 ranges are rounded to pages without 64-bit addition wraparound before entering the physical allocator.
- VGA PCI configuration writes are limited to a verified Bochs/BGA display device and are skipped when the identity does not match.
- Ring-3 tasks use supervisor-inaccessible code/stack pages, a dedicated TSS transition stack and a restricted `int 0x80` ABI. User exceptions terminate the task, reclaim its user pages, and continue scheduling instead of panicking the kernel.
- Paging refuses null physical frames and protects the low identity mapping from accidental kernel unmapping.
- Each ring-3 task has a private page directory with shared kernel mappings; scheduler CR3 switches prevent user mappings from being shared between processes.
- Page-directory creation is tracked in a bounded registry; CR3 switches and destruction reject unregistered physical addresses.
- The paging API rejects user permissions below the protected user mapping floor, preventing identity-mapped kernel pages from being exposed accidentally.
- ELF32 validation and loading reject malformed ranges, integer-overflowable sizes, unsupported machines and writable/executable segments before mapping user pages; loaded pages are zero-initialized and finalized with segment permissions.
- ATA access is bounded, timeout-controlled and rejects capacities beyond the driver’s 28-bit PIO addressing limit. FAT16 support is read-only; no shell command can write arbitrary disk sectors.
- FAT16 mount validates the reserved entries in every mirrored FAT copy before exposing directory/file reads.
- FAT cluster-chain reads compare each consumed FAT entry with every mirrored copy and fail closed on divergence.
- The ATA raw-write path starts write-protected on every boot and remains disabled until a future transactional/journaled storage layer explicitly replaces this policy.
- IPC uses a fixed 16-message, 64-byte-per-message queue with interrupt-safe FIFO operations. `IPC_SEND_TO` binds delivery to an active user generation-PID (while legacy `IPC_SEND` remains broadcast), `IPC_RECEIVE_WAIT` blocks safely until a sender wakes the task, and exited-task messages are purged; ring-3 calls validate all user buffers and return bounded `E2BIG`/`EFAULT`/`EAGAIN` errors.
- Scheduler task IDs include a per-slot generation and are exposed through the bounded `GET_PID` syscall, so a reused slot does not silently retain its previous identity.
- User-process ownership records require both the task slot and its generation-PID during cleanup, preventing a stale slot callback from reclaiming a replacement process.
- The kernel-only `user_process_spawn` path caps image size, requires a kernel address-space context, and records ELF/stack ownership for fail-closed cleanup.
- Generation counters are bounded to the PID encoding width; an exhausted slot is never wrapped into an old identity and is rejected instead (fail-closed).
- Each user address space leaves a page-sized unmapped guard below its user stack; stack underflow therefore terminates the task instead of overwriting another mapping.
- User-process creation scans all bounded stack regions and refuses active-region reuse, so restart timing cannot create duplicate stack metadata or an avoidable spawn failure.
- Stage-2 verifies a build-generated bounded kernel checksum before handoff and the kernel requires the verification bit; this detects corruption but is not a cryptographic secure-boot signature.
- User `EXIT` follows the same ownership-checked cleanup path as faults, reclaiming user mappings before the scheduler can reuse the task slot.
- Terminated task stacks are tracked in a bounded pending-reap mask, so concurrent faults/exits cannot overwrite one another’s deferred cleanup record.
- Scheduler task names are copied into a bounded 15-character buffer; callers cannot leave dangling name pointers or overflow scheduler metadata.
- Diagnostic serial messages are emitted with interrupts disabled so preemptive task switches cannot splice security/test records.

This remains a learning kernel. It has no authentication, secure boot, signed modules, ASLR, SMP isolation, comprehensive validation for every future syscall ABI, or a fully validated persistent filesystem. Do not treat it as a production security boundary until those items are implemented and audited.

Every pull request should pass the LLVM/GCC build, deterministic FAT host test and QEMU ring-3 fault-isolation smoke test defined in `.github/workflows/ci.yml`.
