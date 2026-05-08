# Zenith

A lightweight, hobbyist x86_64 microkernel.

## Current Status
- **Ring 3 Ready**: Successfully transitions from kernel mode to user mode.
- **Syscalls**: Functional system call interface (SYSCALL/SYSRET).
- **Memory**: Paging with identity mapping for kernel and isolated spaces for tasks.
- **Scheduling**: Basic Round-Robin scheduler.

## Building
Requires `nasm` and `llvm-mingw` (or a cross-compiler).

```bash
mingw32-make
```

## Running
Run in QEMU with serial output enabled:

```bash
qemu-system-x86_64 -kernel kernel.bin -serial stdio
```

## Architecture
- **Language**: C and Assembly (NASM).
- **Target**: x86_64 (64-bit).
- **Boot**: Multiboot compliant.
