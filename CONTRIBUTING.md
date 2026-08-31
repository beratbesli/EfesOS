# Contributing to EfesOS

## Before opening a pull request

1. Build the image with `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1`.
2. Run `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1`.
3. Run `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\fat-self-test.ps1` for filesystem changes.
4. Boot the result interactively in QEMU with the `-Run` switch for UI changes.
5. Keep C code freestanding and NASM code compatible with 16-bit BIOS or 32-bit protected mode as appropriate.
6. Do not commit `build/`, `tools/`, local IDE files or generated disk images.
7. Describe the QEMU and host tests performed in the pull request.

## Scope

EfesOS is a small learning kernel. Prefer focused changes that preserve the modular directory layout and avoid adding third-party binary dependencies.
