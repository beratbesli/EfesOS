# Security Policy

## Supported versions

Only the latest commit on `main` is supported.

## Reporting a vulnerability

Do not publish exploit details in a public issue first. Use GitHub's private security advisory feature for this repository and include a minimal reproduction, affected commit and expected impact.

EfesOS is an educational kernel with a deliberately small ring-3 demonstration boundary. Reports about memory corruption, arbitrary control flow, unsafe boot-image handling or toolchain supply-chain risks are still welcome.

## Current security boundary

- Kernel text/rodata are write-protected after paging is enabled; the null page is unmapped and the heap has guard pages and canaries.
- E820 ranges are rounded to pages without 64-bit addition wraparound before entering the physical allocator.
- PMM initialization rejects an invalid or empty linker-reported kernel range before releasing E820 pages, so malformed kernel metadata cannot leave the kernel image allocatable.
- VGA PCI configuration writes are limited to a verified Bochs/BGA display device; the BGA ID is read back before any BAR or command-register write, and mismatches are skipped.
- Ring-3 tasks use supervisor-inaccessible code/stack pages, a dedicated TSS transition stack and a restricted `int 0x80` ABI. User exceptions terminate the task, reclaim its user pages, and continue scheduling instead of panicking the kernel.
- Paging refuses null physical frames and protects the low identity mapping from accidental kernel unmapping.
- Paging treats page tables shared with kernel mappings as immutable from private CR3s: map/protect/unmap operations are rejected, and address spaces are destroyed by physical-table identity, preventing flag-only PDE differences from exposing or freeing shared kernel tables.
- The kernel page directory rejects all user-flagged map/protect operations, so a future internal call cannot create a user-visible kernel PDE that would be copied into new address spaces.
- Each ring-3 task has a private page directory with shared kernel mappings; scheduler CR3 switches prevent user mappings from being shared between processes.
- User-task registration rejects a CR3 already owned by another active user task, preventing accidental address-space reuse through the internal scheduler API.
- Newly allocated user stack frames are zeroed before the task is runnable, preventing physical-page reuse from exposing prior process or kernel data.
- Newly allocated kernel-task stack pages are also zeroed before their initial frame is installed, preventing stale kernel locals from crossing task-slot reuse.
- Page-directory creation is tracked in a bounded registry; CR3 switches and destruction reject unregistered physical addresses.
- The paging API rejects user permissions below the protected user mapping floor, preventing identity-mapped kernel pages from being exposed accidentally.
- ELF32 validation and loading reject malformed ranges, integer-overflowable sizes, unsupported machines and writable/executable segments before mapping user pages; loaded pages are zero-initialized and finalized with segment permissions.
- ELF validation also requires the canonical 32-bit header size and rejects virtual addresses at or above the user ceiling before unsigned range arithmetic, preventing high-address wraparound during loading.
- ELF segment page accounting rejects requests exceeding the bounded 1024-page image budget before subtraction, preventing unsigned-capacity wraparound and avoidable allocator exhaustion.
- ELF executable pages carry a software execute bit, and scheduler plus syscall boundaries reject a ring-3 EIP that is not on a user executable page; this narrows data/stack execution even though hardware NX is unavailable in non-PAE mode.
- The paging API enforces W^X for every mapping and protection operation, rejecting writable+executable combinations before they reach a page table.
- ATA access is bounded, timeout-controlled and rejects capacities beyond the driver’s 28-bit PIO addressing limit. FAT16 support is read-only; no shell command can write arbitrary disk sectors.
- FAT16 mount validates the reserved entries in every mirrored FAT copy before exposing directory/file reads.
- FAT cluster-chain reads compare each consumed FAT entry with every mirrored copy and fail closed on divergence.
- The ATA raw-write path starts write-protected on every boot and remains disabled until a future transactional/journaled storage layer explicitly replaces this policy.
- GitHub Actions dependencies are pinned to a verified commit and checkout credentials are not persisted in the worktree, reducing CI supply-chain and token-leakage risk.
- Dependabot is configured to propose weekly GitHub Actions updates so pinned workflow dependencies receive security fixes without reverting to mutable tags.
- PowerShell build and self-test scripts accept PATH-resolved tools only when `Get-Command` reports a real `Application` with a non-empty source; profile functions and aliases cannot silently replace compiler or QEMU binaries.
- Boot self-test invokes the raw write API while protection is active and requires the call to fail, guarding the write-protected invariant against future call-path regressions.
- IPC uses a fixed 16-message, 64-byte-per-message queue with interrupt-safe FIFO operations. `IPC_SEND_TO` binds delivery to an active user generation-PID (while legacy `IPC_SEND` remains broadcast), `IPC_RECEIVE_WAIT` blocks safely until a sender wakes the task, and exited-task messages are purged; ring-3 calls validate all user buffers and return bounded `E2BIG`/`EFAULT`/`EAGAIN` errors.
- Scheduler task IDs include a per-slot generation and are exposed through the bounded `GET_PID` syscall, so a reused slot does not silently retain its previous identity.
- User-process ownership records require both the task slot and its generation-PID during cleanup, preventing a stale slot callback from reclaiming a replacement process.
- User-stack cleanup compares the unmapped physical frame with the recorded owner before freeing it, preventing corrupted metadata from releasing an unexpected frame.
- ELF image cleanup preflights every owned page and rejects missing mappings instead of silently reporting a partial/unowned unload as successful.
- Partial ELF-load rollback panics on an unexpected unmap failure instead of freeing a still-mapped frame or hiding a resource leak.
- The kernel-only `user_process_spawn` path caps image size, requires a kernel address-space context, and records ELF/stack ownership for fail-closed cleanup.
- The shell’s `run NAME` path reads only through bounded FAT APIs and routes the image through the same ELF validation, W^X mapping, zeroed stack and ownership cleanup before it becomes runnable.
- User-task registration also requires the caller to be in the kernel address space and the user stack top to be page-aligned, preventing malformed context creation through internal APIs.
- Generation counters are bounded to the PID encoding width; an exhausted slot is never wrapped into an old identity and is rejected instead (fail-closed).
- Each user address space leaves a page-sized unmapped guard below its user stack; stack underflow therefore terminates the task instead of overwriting another mapping.
- User-process spawn verifies after ELF loading that the reserved stack-guard page is still unmapped; an ELF segment cannot silently consume the guard and weaken overflow isolation.
- A boot self-test attempts exactly such a guard-overlapping ELF and requires the spawn to fail with complete resource cleanup.
- User-process creation scans all bounded stack regions and refuses active-region reuse, so restart timing cannot create duplicate stack metadata or an avoidable spawn failure.
- Stage-2 verifies a build-generated CRC-32 kernel digest before handoff and the kernel requires the verification bit; this detects accidental/tampering corruption better than a sum but is not a cryptographic secure-boot signature.
- User `EXIT` follows the same ownership-checked cleanup path as faults, reclaiming user mappings before the scheduler can reuse the task slot.
- Terminated task stacks are tracked in a bounded pending-reap mask, so concurrent faults/exits cannot overwrite one another’s deferred cleanup record.
- Deferred kernel-stack cleanup verifies the unmapped guard and every recorded physical frame before freeing it; missing or mismatched ownership is fail-closed instead of silently leaking or freeing an unexpected page.
- Scheduler dispatch fails closed when no runnable task exists instead of returning a terminated or blocked task’s saved frame.
- Scheduler task names are copied into a bounded 15-character buffer; callers cannot leave dangling name pointers or overflow scheduler metadata.
- Diagnostic serial messages are emitted with interrupts disabled so preemptive task switches cannot splice security/test records.
- All 256 IDT vectors now have a kernel stub; unexpected high vectors reach the common fail-closed dispatcher instead of an empty descriptor/triple fault, while only `int 0x80` is user-callable.
- The breakpoint self-test is accepted only from ring 0; a ring-3 `int3` now follows the isolated user-fault cleanup path instead of continuing as a privileged diagnostic.

This remains a learning kernel. It has no authentication, secure boot, signed modules, ASLR, SMP isolation, hardware-enforced NX (the current 32-bit non-PAE paging mode has no NX bit), comprehensive validation for every future syscall ABI, or a fully validated persistent filesystem. Do not treat it as a production security boundary until those items are implemented and audited.

Every pull request should pass the LLVM/GCC build, deterministic FAT host test and QEMU ring-3 fault-isolation smoke test defined in `.github/workflows/ci.yml`.
