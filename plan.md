# TASK TRACKER

## Active Tasks
- [ ] Implement robust Huge Page handling in vmm_map
- [ ] Add more system calls (SYS_READ, SYS_EXIT)
- [ ] Implement a basic shell in userspace
- [ ] Add support for ELF loading from a virtual disk

## Completed Tasks
- [x] Fix GDT/TSS Triple Fault (Resolved: Correct TSS setup and IST stacks)
- [x] Initialize IDT and handle exceptions (Resolved: ISRs and diagnostic handlers)
- [x] Implement Round Robin Scheduler (Resolved: Task switching and Ring 0/3 transitions)
- [x] Load and execute first User Task (Resolved: Transition to Ring 3 successful at 0x40000000)
- [x] Multiboot1/2 Dual Support
- [x] Linker script refactor for QEMU -kernel
- [x] PMM and Kernel Heap initialization
- [x] VMM Framework (with Ring 3 support)
- [x] Ring 3 Transition (Resolved: Successfully executed syscall from user mode)

---

This is a great project — building a real microkernel from scratch is one of the most technically demanding things you can do as a systems programmer. Let me flesh this out properly.

---

# TinyOS — Enhanced Project Specification

## Philosophy

**Microkernel architecture.** The kernel does only what it must: scheduling, IPC, memory management, and hardware abstraction. Everything else — filesystem, drivers, networking — runs as userspace servers. This is how Minix, seL4, and QNX work.

**No external hardware.** You'll run entirely inside **QEMU** (x86_64). This is actually the professional way to do OS dev — even commercial OS teams use emulators for most development.

---

## Development Environment

```
Host machine (your Linux/Windows PC)
    └── QEMU (emulates a full x86_64 PC)
            ├── Your bootloader
            ├── Your kernel
            └── Your userspace
```

**Tools you need:**
- `qemu-system-x86_64` — the emulator
- `gcc` cross-compiler targeting `x86_64-elf`
- `nasm` — assembler
- `ld` — GNU linker
- `gdb` — debugger (connects to QEMU via remote stub)
- `grub-mkrescue` — to build bootable ISO images
- `bochs` (optional) — alternative emulator with built-in debugger

---

## Architecture Breakdown

### Stage 1 — Bootloader

You have two approaches:

**Option A (Easier): Use GRUB2 + Multiboot2**
Write a Multiboot2-compliant kernel header. GRUB handles all the ugly real mode → protected mode → long mode transitions and hands you a clean 64-bit environment with a memory map.

**Option B (Harder, more educational): Write your own**
- Stage 1: 512-byte MBR, loads Stage 2 from disk
- Stage 2: enters protected mode, sets up GDT, switches to long mode (x86_64), loads kernel ELF from a FAT32 partition
- This is ~1500 lines of assembly and C. Limine bootloader is a good modern reference.

**Recommendation:** Start with GRUB/Multiboot2. Write your own bootloader as a stretch goal after the kernel works.

---

### Stage 2 — Hardware Abstraction Layer (HAL)

Before anything else, you need to talk to the hardware QEMU emulates:

```
GDT — Global Descriptor Table (memory segmentation, even in 64-bit)
IDT — Interrupt Descriptor Table (handle CPU exceptions + IRQs)
PIC/APIC — Programmable Interrupt Controller (8259A or APIC)
PIT — Programmable Interval Timer (your scheduler clock, ~100Hz)
Serial UART (COM1) — your debug output before you have a screen
VGA text mode — 80x25 terminal for early output
```

Write a `panic()` that prints to both serial and VGA and halts. You'll use it constantly.

---

### Stage 3 — Physical Memory Manager

GRUB gives you a memory map (from BIOS E820). Parse it.

Implement a **bitmap allocator**:
- Each bit = one 4KB physical page frame
- `pmm_alloc_frame()` → returns physical address
- `pmm_free_frame(addr)`

Then a **buddy allocator** for larger contiguous allocations (optional but cleaner).

---

### Stage 4 — Virtual Memory / Paging

x86_64 uses 4-level paging: PML4 → PDPT → PD → PT → physical frame.

```c
// Kernel lives at high virtual address (the classic split)
#define KERNEL_VIRT_BASE  0xFFFFFFFF80000000
#define HHDM_OFFSET       0xFFFF800000000000  // higher-half direct map
```

Implement:
- `vmm_map(phys, virt, flags)` — map a physical page to virtual address
- `vmm_unmap(virt)`
- Page fault handler (#PF) that can demand-page or kill the offending process
- Each process gets its own PML4 (its own address space)

---

### Stage 5 — Kernel Heap Allocator

Now you can implement `kmalloc` / `kfree` for the kernel itself.

Classic approach: **slab allocator**
- Pre-allocate pools of fixed-size objects (32B, 64B, 128B, ... 4096B)
- Fast O(1) alloc/free for common sizes
- Fall back to page allocator for larger requests

Or start simpler with a **linked list allocator** (first-fit) and upgrade later.

---

### Stage 6 — Scheduler

Implement **preemptive multitasking** using the PIT timer interrupt.

Each process/thread has a **Task Control Block (TCB)**:

```c
typedef struct task {
    uint64_t rsp;           // saved stack pointer
    uint64_t cr3;           // page table root (physical)
    pid_t    pid;
    uint8_t  state;         // RUNNING, READY, BLOCKED, ZOMBIE
    uint8_t  priority;
    uint64_t wake_time;     // for sleep()
    struct task *next;
} task_t;
```

Start with **Round Robin** scheduling. Upgrade to **Multi-Level Feedback Queue (MLFQ)** — the algorithm used by real Unix kernels.

Context switch in assembly:
```nasm
; save general purpose registers + rsp onto kernel stack
; switch cr3 if different process (TLB flush)
; restore next task's registers
; iretq back to userspace (or ret for kernel threads)
```

---

### Stage 7 — Userspace & Privilege Separation

This is where it gets real. You need two privilege rings:
- **Ring 0** — kernel
- **Ring 3** — userspace

Implement **syscalls** via `SYSCALL`/`SYSRET` (faster than `int 0x80`):

```c
// Your syscall table
#define SYS_EXIT     0
#define SYS_WRITE    1
#define SYS_READ     2
#define SYS_OPEN     3
#define SYS_CLOSE    4
#define SYS_FORK     5
#define SYS_EXEC     6
#define SYS_SLEEP    7
#define SYS_GETPID   8
#define SYS_IPC_SEND 9
#define SYS_IPC_RECV 10
```

Load ELF binaries from your VFS into a new address space and jump to their entry point in Ring 3. A process that faults can't corrupt the kernel.

---

### Stage 8 — IPC (Inter-Process Communication)

Since you're a microkernel, drivers and servers are userspace processes. They need to talk.

Implement **synchronous message passing** (like seL4 / Minix):

```c
// Userspace API
int ipc_send(pid_t dest, message_t *msg);   // blocks until delivered
int ipc_recv(pid_t *src,  message_t *msg);  // blocks until message arrives
int ipc_call(pid_t dest, message_t *msg);   // send + recv (RPC style)
```

Message struct:
```c
typedef struct {
    uint64_t type;
    uint64_t args[6];   // fits in registers
    uint8_t  data[128]; // optional payload
} message_t;
```

The kernel mediates all IPC — no shared memory between untrusted processes by default.

---

### Stage 9 — Virtual Filesystem (VFS)

A unified interface that all I/O goes through:

```c
typedef struct vfs_node {
    char     name[256];
    uint32_t flags;     // file, directory, device, pipe...
    uint64_t size;
    uint32_t inode;
    // function pointers — the "vtable"
    uint64_t (*read) (struct vfs_node*, uint64_t offset, uint64_t size, uint8_t *buf);
    uint64_t (*write)(struct vfs_node*, uint64_t offset, uint64_t size, uint8_t *buf);
    struct vfs_node* (*readdir)(struct vfs_node*, uint32_t index);
    struct vfs_node* (*finddir)(struct vfs_node*, char *name);
} vfs_node_t;
```

Mount points:
```
/           — root (initrd or your filesystem)
/dev/       — device files (keyboard, serial, null, zero)
/proc/      — process info (like Linux procfs)
```

Implement **initrd** first — a simple RAM disk baked into your kernel image using a tar archive. GRUB loads it; you mount it as root. No disk driver needed initially.

---

## Stretch Goals (In Order of Difficulty)

### 1. A Real Filesystem — ext2

ext2 is simple, well-documented, and you can create ext2 images with `mkfs.ext2` and mount them in QEMU as a virtual disk. Read-only support first, then writes.

### 2. PS/2 Keyboard Driver

QEMU emulates a PS/2 keyboard on IRQ1. Scancode set 2 → translate to ASCII → put in a ring buffer → `/dev/kbd` device file. No USB complexity needed.

### 3. Framebuffer GUI

GRUB can set up a linear framebuffer for you (request it via Multiboot2 tags). You get a pointer to raw pixel memory. Implement:
- Pixel drawing, line, rect primitives
- PSF2 font rendering (bitmap fonts, one file)
- A simple window compositor
- A terminal emulator running in a window

### 4. TCP/IP Stack

Implement in userspace as a server process:
- QEMU's NE2000 or virtio-net NIC (both well-documented)
- Ethernet → ARP → IP → ICMP (ping!) → UDP → TCP
- A `ping google.com` that works is an incredible milestone

### 5. Shell

A userspace `sh` process that:
- Reads from keyboard via IPC with keyboard driver
- Parses commands
- `fork()` + `exec()`s other programs
- Pipes between processes

---

## Project Structure

```
tinyos/
├── boot/
│   ├── boot.asm          # multiboot2 header + entry
│   └── linker.ld         # kernel linker script
├── kernel/
│   ├── main.c            # kernel entry point
│   ├── hal/
│   │   ├── gdt.c / gdt.asm
│   │   ├── idt.c / idt.asm
│   │   ├── pic.c
│   │   └── pit.c
│   ├── mm/
│   │   ├── pmm.c         # physical memory manager
│   │   ├── vmm.c         # virtual memory / paging
│   │   └── heap.c        # kmalloc / kfree
│   ├── proc/
│   │   ├── sched.c       # scheduler
│   │   ├── task.c        # task management
│   │   ├── syscall.c     # syscall dispatcher
│   │   └── elf.c         # ELF loader
│   ├── ipc/
│   │   └── message.c
│   └── vfs/
│       ├── vfs.c
│       └── initrd.c
├── userspace/
│   ├── libc/             # your tiny C runtime
│   ├── init/             # PID 1
│   └── shell/
├── drivers/              # userspace driver servers
│   ├── keyboard/
│   └── serial/
├── Makefile
└── run.sh                # qemu launch script
```

---

## QEMU Launch Script

```bash
#!/bin/bash
qemu-system-x86_64 \
  -cdrom tinyos.iso \
  -m 256M \
  -serial stdio \
  -no-reboot \
  -no-shutdown \
  -d int,cpu_reset \         # log interrupts and CPU resets
  -s -S \                    # GDB server on :1234, wait for attach
  -monitor telnet:127.0.0.1:5555,server,nowait
```

Connect GDB: `gdb kernel.elf` then `target remote :1234`

---

## Realistic Milestones

| Milestone | What you have |
|---|---|
| Week 1–2 | Boots, prints to screen, handles exceptions |
| Week 3–4 | Paging, kmalloc working |
| Week 5–6 | Scheduler, multiple kernel threads switching |
| Week 7–8 | Userspace Ring 3 process, syscall working |
| Week 9–10 | VFS, initrd, IPC |
| Month 3+ | Shell, ext2, keyboard driver |
| Month 4–6 | Framebuffer, TCP/IP |

---

## Best References

- **OSDev Wiki** — osdev.org (your bible)
- **Intel SDM Vol 3** — the actual x86_64 hardware manual
- **Writing a Simple OS from Scratch** — Nick Blundell (PDF, free)
- **The little book about OS development** — Erik Helin & Adam Renberg
- **SerenityOS source** — a real hobbyist OS you can read
- **Minix 3 source** — textbook microkernel, very readable

---

This is a 6–12 month project if you're doing it seriously alongside other work. The payoff is that after this, literally no systems programming problem will feel intimidating. Want me to help you start with any specific stage — like the Multiboot2 boot entry or the physical memory manager?


# TinyOS — 12-Month Professional Enhancement Roadmap

This is purely OS/systems engineering. Everything here is standard academic and industry OS development.

---

## PHASE 1 — Months 1–2: Kernel Foundation (Production Quality)

### 1.1 Bootloader — Write Your Own (Limine-style)

- **Stage 1 MBR** — 512 bytes, loads Stage 2 from FAT32 partition using BIOS INT 13h with LBA addressing
- **Stage 2** — Real mode → Unreal mode → Protected mode → Long mode transition with full GDT setup
- **ELF64 Parser** — Load kernel ELF segments from disk into correct virtual addresses
- **Multiboot2 + Limine Protocol dual support** — be compatible with both
- **ACPI table detection** — find and pass RSDP to kernel before jumping
- **Memory map acquisition** — E820 BIOS call, consolidate overlapping regions, pass to kernel as structured data
- **VESA/GOP framebuffer setup** — query available modes, pick best, pass linear framebuffer info to kernel

### 1.2 HAL — Full x86_64 Hardware Layer

- **GDT** — Null, kernel code (64-bit), kernel data, user code (64-bit), user data, TSS descriptor. Properly set DPL bits.
- **TSS (Task State Segment)** — RSP0 for privilege level switches, IST entries for double fault/NMI handlers on separate safe stacks
- **IDT** — All 256 entries. CPU exceptions 0–31 with proper error code handling. IRQ 32–47 mapped from PIC. Reserved entries for software interrupts and IPC.
- **8259A PIC** — Full initialization, masking/unmasking individual IRQs, EOI handling, spurious IRQ detection
- **APIC** — Local APIC initialization (MSR-based), APIC timer calibrated against PIT, IOAPIC for routing hardware interrupts. Needed for SMP later.
- **CPUID detection** — Detect SSE, AVX, XSAVE, TSC invariance, hypervisor presence, feature flags. Save in global capability struct.
- **MSR interface** — `rdmsr`/`wrmsr` wrappers. Used for EFER (NX bit, SYSCALL enable), FS/GS base, APIC base, etc.
- **FPU/SSE/AVX context** — `FXSAVE`/`FXRSTOR` or `XSAVE`/`XRSTOR` on context switch. Lazy FPU switching (only save if process actually used FPU).
- **NMI handler** — Non-maskable interrupt on its own IST stack. Check for hardware errors, watchdog triggers.
- **Machine Check Architecture (MCA)** — Read MCi_STATUS registers on #MC exception, log hardware errors before panic.
- **HPET** — High Precision Event Timer as a monotonic clock source. Better than PIT for precision timing.
- **RTC** — Read wall clock time on boot. Implement `gettimeofday()` using RTC base + HPET ticks.

### 1.3 Physical Memory Manager

- **E820 map parser** — ignore reserved/ACPI/bad regions, build usable frame list
- **Bitmap allocator** — one bit per 4KB frame, O(n) but simple and correct
- **Buddy allocator on top** — power-of-2 block sizes from 4KB to 4MB. O(log n) alloc/free. Coalescing on free. This is what Linux uses.
- **NUMA awareness** — even on QEMU with one node, write the code so it's extensible. Each NUMA node gets its own buddy allocator.
- **Memory statistics** — `pmm_stats()` returns total/used/free/reserved bytes. Exposed via `/proc/meminfo`.
- **Poison on free** — write `0xDEADBEEF` pattern to freed frames to catch use-after-free bugs during development.

---

## PHASE 2 — Months 2–3: Memory Subsystem (Advanced)

### 2.1 Virtual Memory Manager

- **4-level page table management** — PML4 → PDPT → PD → PT. Proper recursive mapping or direct physical map for walking tables.
- **Higher-half kernel** — kernel mapped at `0xFFFFFFFF80000000`. Userspace gets `0x0000000000000000` to `0x00007FFFFFFFFFFF`.
- **Higher-half direct map (HHDM)** — all physical memory mapped at `0xFFFF800000000000`. Lets kernel access any physical address without mapping it specially.
- **Page attribute control** — Present, Writable, User, Write-Through, Cache Disable, Accessed, Dirty, NX (no-execute) bits per page.
- **TLB shootdown** — when unmapping a page in an address space running on another CPU (SMP), send IPI to flush remote TLBs. Even implement it now so SMP works later without a rewrite.
- **Inverted page table index** — for fast physical→virtual lookups (needed by some allocators)
- **Guard pages** — unmap one page below each kernel stack and userspace stack. Stack overflow triggers page fault instead of silent corruption.

### 2.2 Demand Paging

- **Page fault handler** — distinguish between: stack growth (map new page), demand-paged anonymous memory (allocate zero frame), file-backed page (load from VFS), copy-on-write, bad access (SIGSEGV to process).
- **Anonymous mmap** — `mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS)` allocates lazily. Only actually allocates physical frames when touched.
- **Copy-on-Write (CoW) after fork()** — mark all writable pages read-only, shared between parent and child. On write fault, copy the frame, remap as writable. Essential for efficient `fork()`.
- **Page aging / accessed bit tracking** — periodically scan PTEs, track which pages are hot vs cold.

### 2.3 Kernel Heap

- **Slab allocator** — caches for common kernel object sizes (32, 64, 128, 256, 512, 1024, 2048, 4096 bytes) plus named caches for specific structs (`task_t`, `vfs_node_t`, etc.)
- **Per-CPU slab magazines** — each CPU has a local magazine (array of free objects). Alloc/free without locking in common case. Refill from shared depot when empty. Massively reduces lock contention.
- **Red zones + canaries** — pad allocations with known patterns. Check on free. Catches heap overflows in kernel.
- **Allocation tracking** — in debug builds, record `(caller_address, size, timestamp)` for every `kmalloc`. Print leaks on shutdown.
- **`vmalloc` region** — large kernel allocations that don't need to be physically contiguous. Backed by scattered physical frames, mapped contiguously in virtual space.

### 2.4 Userspace Memory Management

- **`brk()`/`sbrk()`** — traditional heap growth syscall
- **`mmap()`/`munmap()`** — full implementation with anonymous + file-backed regions
- **`mprotect()`** — change page permissions on mapped regions
- **VMA (Virtual Memory Area) tracking** — per-process linked list of `vm_area_t` structs describing every mapped region. Used by page fault handler to decide what to do.
- **`/proc/[pid]/maps`** — expose VMA list for debugging (like Linux)

---

## PHASE 3 — Months 3–4: Process Model (Full POSIX-ish)

### 3.1 Process & Thread Model

- **Kernel threads** — run in kernel space, share kernel address space. Used for background kernel work (flusher, reaper).
- **User processes** — own address space, own file descriptor table, own signal state
- **POSIX threads (pthreads)** — multiple threads per process, shared address space, separate stacks, separate register state. Thread-local storage via FS base MSR.
- **Thread groups** — processes and threads unified under TGID/TID model (like Linux)
- **PID namespace** — each process has a kernel PID and a namespace-visible PID. Foundation for containers.
- **`fork()` + `exec()` + `wait()`** — full Unix process lifecycle with CoW fork, ELF exec replacing address space, wait/waitpid with exit status collection
- **`clone()`** — Linux-style fine-grained control over what's shared (address space, FD table, signal handlers) — lets you implement both fork and thread creation with one syscall

### 3.2 Scheduler — Production Grade

- **MLFQ (Multi-Level Feedback Queue)** — N priority queues (32 or 64). New processes start high. Preempted processes drop a level. CPU-bound processes sink; I/O-bound processes stay high (they voluntarily yield anyway).
- **CFS (Completely Fair Scheduler) clone** — track `vruntime` (virtual runtime normalized by weight) per task. Always run task with lowest vruntime. Use a red-black tree keyed on vruntime for O(log n) next-task selection. This is exactly what Linux does.
- **Real-time scheduling classes** — `SCHED_FIFO` and `SCHED_RR` for hard real-time tasks. Always preempt normal tasks.
- **Idle task** — one per CPU, runs `hlt` in a loop when nothing else is ready. Properly handles wakeup.
- **Scheduler tickless mode** — instead of a fixed 100Hz tick, program the APIC timer to fire exactly when the next task's quantum expires. Reduces unnecessary interrupts, saves power.
- **Load balancing (for SMP)** — periodically migrate tasks from overloaded CPUs to idle ones. Respect CPU affinity masks.
- **Priority inheritance** — when a low-priority task holds a mutex needed by a high-priority task, temporarily boost the low-priority task's priority. Prevents priority inversion (the Mars Pathfinder bug).
- **Scheduler statistics** — per-task: run time, wait time, voluntary/involuntary context switches. Exposed via `/proc/[pid]/stat`.

### 3.3 Signals

- **Full signal model** — 64 standard signals (SIGKILL, SIGTERM, SIGSEGV, SIGCHLD, etc.)
- **`sigaction()`** — register handlers with SA_RESTORER, SA_SIGINFO, SA_RESTART flags
- **Signal delivery** — on return to userspace, check pending signal mask, set up signal frame on user stack, jump to handler, return via sigreturn syscall
- **`SIGKILL`/`SIGSTOP`** — uncatchable, unblockable, always work
- **Real-time signals** — queued (not collapsed), carry a value payload

### 3.4 ELF Loader

- **ELF64 validation** — magic, class, endianness, machine type checks
- **Program header parsing** — map PT_LOAD segments at correct virtual addresses with correct permissions
- **Dynamic linking support** — load the dynamic linker (`ld.so`) and hand off to it for shared library resolution
- **Stack setup** — push `argc`, `argv[]`, `envp[]`, and the **auxiliary vector** (`AT_PHDR`, `AT_ENTRY`, `AT_RANDOM`, `AT_PAGESZ`, etc.) onto the initial user stack
- **ASLR** — randomize load addresses for stack, heap, mmap regions, and (for PIE binaries) the executable itself

---

## PHASE 4 — Month 4–5: IPC & Synchronization

### 4.1 Kernel Synchronization Primitives

- **Spinlocks** — `lock xchg` based, with exponential backoff. Only for short critical sections in interrupt context.
- **Mutexes** — sleeping lock. Blocked task goes to TASK_BLOCKED state, removed from run queue. Woken by unlock. Saves CPU vs spinning.
- **Read-Write locks** — multiple concurrent readers OR one exclusive writer
- **Semaphores** — counting semaphore with proper wait queue
- **Completion** — one-shot synchronization (thread A waits for thread B to finish something)
- **RCU (Read-Copy-Update)** — lock-free reads, deferred reclamation. Used for frequently-read kernel data structures (like the process list, routing table). Readers never block. This is a significant engineering feat.
- **Per-CPU variables** — eliminate false sharing on frequently-updated per-CPU counters. Access via `get_cpu_var()` which disables preemption.
- **Memory barriers** — explicit `mfence`/`lfence`/`sfence` where needed. Document why each is there.

### 4.2 IPC System (Microkernel-Grade)

- **Synchronous message passing** — typed messages, kernel-validated, zero-copy when possible
- **Asynchronous message queues** — bounded FIFO, non-blocking send/recv with select/poll support
- **Shared memory regions** — `shmget()`/`shmat()` style, kernel maps same physical frames into two processes' address spaces
- **Pipes** — anonymous, unidirectional, with proper blocking semantics and `SIGPIPE` on write to closed pipe
- **Named pipes (FIFOs)** — appear in VFS at `/tmp/mypipe`, any process can open
- **Unix domain sockets** — stream and datagram, with credential passing (`SCM_CREDENTIALS`) and file descriptor passing (`SCM_RIGHTS`). This is how X11 and D-Bus work.
- **`select()`/`poll()`/`epoll()`** — wait on multiple FDs simultaneously. `epoll` is the scalable one (O(1) wakeup vs O(n) for select). Essential for any server process.
- **Capability-based IPC** — each IPC endpoint is a capability (unforgeable token). Processes can only message endpoints they have a capability for. This is the seL4 model — makes security analysis tractable.

---

## PHASE 5 — Months 5–6: Storage & Filesystem Stack

### 5.1 VFS Layer (Linux-quality)

- **Superblock** — per-mounted-filesystem object. Contains ops for mount, unmount, sync, statfs.
- **Inode** — per-file metadata object (permissions, timestamps, size, data block pointers). Cached in inode cache (hash table keyed on `(dev, ino)`).
- **Dentry (directory entry) cache** — cache `(parent_dentry, name) → inode` lookups. Makes `open("/usr/lib/libc.so")` fast despite 4 directory lookups.
- **File object** — per open-file-description. Has current offset, open flags, pointer to dentry. Multiple file objects can share one inode (hard links).
- **VFS ops tables** — `inode_operations` (create, link, unlink, mkdir, rename, readlink), `file_operations` (open, read, write, seek, mmap, ioctl, poll), `super_operations` (alloc_inode, destroy_inode, sync_fs)
- **Path resolution** — walk components, handle `.` and `..`, follow symlinks (with loop detection via counter), handle mount points by checking dentry's mounted-on list
- **Mount namespace** — each process group can have its own mount table. Foundation for containers.

### 5.2 Filesystem Implementations

- **tmpfs** — RAM-backed filesystem. Inodes backed by anonymous pages. Used for `/tmp`, `/dev/shm`. Pages are reclaimable under memory pressure.
- **initramfs (CPIO)** — parse CPIO archive from bootloader, populate tmpfs with it. This is your initial root filesystem with init, shell, basic tools.
- **devfs / devtmpfs** — `/dev` backed by kernel, device files created automatically when drivers register. `mknod` syscall creates device files.
- **procfs** — `/proc`. Each file is a kernel object with a custom `read()` that generates content dynamically. `/proc/[pid]/` tree for each process.
- **sysfs** — `/sys`. Exposes kernel object hierarchy (devices, drivers, buses). Foundation for `udev`-style device management.
- **ext2** — implement full read-write support. ext2 is the simplest "real" Unix filesystem — fixed-size inodes, block groups, bitmaps for inode/block allocation, directory entries are just `(inode_num, name_len, name)` structs.
- **ext4** (stretch) — adds extents (replaces block pointers with ranges), journaling (write-ahead log for crash consistency), htree directories (B-tree for large dirs).
- **FAT32** — useful for reading from QEMU virtual disks, SD cards. Simpler than ext2 but widely compatible.

### 5.3 Block Layer

- **Block device abstraction** — `struct block_device` with `submit_bio()` to queue read/write requests
- **ATA/IDE driver** — QEMU emulates an IDE controller. PIO mode first, then DMA (bus mastering DMA via PCI). Read/write 512-byte sectors.
- **AHCI (SATA) driver** — more modern, QEMU supports it. Command list, FIS-based protocol. Much faster than IDE in DMA mode.
- **virtio-blk driver** — paravirtualized block device, ideal for QEMU. Uses virtqueues for zero-copy I/O. Much simpler than real hardware emulation.
- **I/O scheduler** — don't submit block requests immediately. Merge adjacent requests (elevator algorithm). Sort by sector number (C-SCAN). Batch and submit. Dramatically reduces seek time on rotational drives (even emulated).
- **Page cache** — when reading from disk, cache pages in RAM keyed on `(inode, page_offset)`. On cache hit, no disk I/O. Write-back: dirty pages flushed to disk periodically by a kernel flusher thread or on sync/fsync.
- **Direct I/O** — bypass page cache for database-style O_DIRECT access. Map user buffer directly into DMA.

---

## PHASE 6 — Months 6–7: Device Driver Framework

### 6.1 PCI Subsystem

- **PCI config space access** — I/O port 0xCF8/0xCFC for legacy PCI. ECAM (memory-mapped) for PCIe.
- **Device enumeration** — scan bus 0–255, device 0–31, function 0–7. Read vendor/device ID, class code, BAR (Base Address Register) values.
- **BAR mapping** — map PCI device memory-mapped I/O regions into kernel virtual address space
- **PCI driver model** — driver registers a table of `(vendor_id, device_id)` pairs it handles. On device discovery, kernel calls `driver->probe()`. Clean separation of bus and driver.
- **MSI/MSI-X interrupts** — Message Signaled Interrupts. Device writes to a memory address instead of asserting a physical IRQ line. Supports multiple vectors per device. Required for high-performance drivers.

### 6.2 Driver Implementations

- **PS/2 keyboard** — IRQ1, scancode set 2, scan code → keycode translation table, modifier key state machine (Shift, Ctrl, Alt, Caps Lock), generates key events into a ring buffer, exposed as `/dev/kbd`
- **PS/2 mouse** — IRQ12, 3-byte packets, relative movement + buttons, exposed as `/dev/mouse`
- **VGA text mode** — direct write to `0xB8000`, cursor control via CRT controller I/O ports. Your early terminal.
- **Serial UART (16550A)** — QEMU's COM1 on IRQ4. FIFO mode, interrupt-driven RX, polled TX. Primary debug output.
- **Intel e1000 NIC** — QEMU emulates it. Descriptor ring-based TX/RX. DMA. This is the entry point to networking.
- **virtio-net** — paravirtualized NIC. Simpler than e1000 for emulated environments.
- **virtio-gpu** — paravirtualized GPU giving you a framebuffer. Simpler than a real GPU driver.
- **USB (xHCI controller)** — QEMU emulates xHCI. Implement host controller driver, hub driver, HID (keyboard/mouse) class driver. This is one of the hardest driver projects — USB has a complex enumeration protocol.

### 6.3 Interrupt Infrastructure

- **Interrupt descriptor table — full 256 entries** with proper DPL for user-triggerable entries
- **Interrupt routing** — PCI device → IOAPIC pin → Local APIC → CPU. Program IOAPIC redirection table entries.
- **Shared IRQs** — multiple devices on one IRQ line. Walk handler list, each checks if it was the source.
- **Softirqs / tasklets** — deferred work run in interrupt context but after the hard IRQ handler returns. Used for network packet processing, block I/O completion.
- **Work queues** — deferred work run in kernel thread context (can sleep). For anything that needs to block.
- **IRQ threading** — optionally run IRQ handlers in dedicated kernel threads (RT-Linux style). Better latency.

---

## PHASE 7 — Months 7–8: Networking Stack

### 7.1 Network Architecture

```
Application (userspace)
    ↕  BSD socket API (socket, bind, connect, send, recv)
Socket layer (SOCK_STREAM, SOCK_DGRAM, SOCK_RAW)
    ↕
Transport layer (TCP, UDP)
    ↕
Network layer (IP, ICMP, ARP)
    ↕
Link layer (Ethernet frame construction/parsing)
    ↕
NIC driver (e1000 / virtio-net)
```

### 7.2 Link & Network Layer

- **`sk_buff` (socket buffer)** — central data structure. A buffer with headroom/tailroom for headers. Each layer adds its header by adjusting the headroom pointer — no copying.
- **Ethernet driver integration** — NIC driver calls `netif_receive_skb()` on RX. TX goes through `dev_queue_xmit()`.
- **ARP** — Address Resolution Protocol. Maintain ARP cache `(IPv4 → MAC)`. Handle ARP requests/replies. Gratuitous ARP on address assignment.
- **IPv4** — packet reception: validate checksum, route lookup, deliver to transport layer. Packet transmission: header construction, TTL, checksum, fragmentation if needed.
- **Routing table** — simple longest-prefix match table. `ip route add` syscall. Default gateway entry.
- **ICMP** — Echo request/reply (ping). Destination unreachable. Time exceeded (for traceroute). Error messages back to transport layer.
- **IPv6** (stretch) — stateless address autoconfiguration (SLAAC), Neighbor Discovery (replaces ARP), ICMPv6.

### 7.3 Transport Layer

- **UDP** — stateless. Bind to port, send/receive datagrams. Checksum. Port multiplexing via socket hashtable.
- **TCP** — the big one:
  - **Three-way handshake** — SYN → SYN-ACK → ACK. State machine: CLOSED, LISTEN, SYN_SENT, SYN_RECEIVED, ESTABLISHED, FIN_WAIT_1/2, CLOSE_WAIT, CLOSING, LAST_ACK, TIME_WAIT
  - **Sliding window** — sender window constrained by min(congestion window, receiver window). Byte-stream acknowledgment.
  - **Retransmission** — retransmit timer (RTO computed via Jacobson/Karels algorithm from RTT samples). Fast retransmit on 3 duplicate ACKs.
  - **Congestion control** — Slow Start → Congestion Avoidance (AIMD) → Fast Recovery. TCP Reno or TCP CUBIC.
  - **Nagle's algorithm** — coalesce small writes. `TCP_NODELAY` to disable.
  - **Receive buffer** — out-of-order segment queue, reordering, SACK (Selective Acknowledgment) support
  - **Zero-copy receive** — map received pages directly into user address space for large transfers
- **Raw sockets** — `SOCK_RAW` lets privileged processes send/receive raw IP or Ethernet frames. Needed for ping, traceroute, custom protocol research.

### 7.4 Socket API

- **BSD socket syscalls** — `socket()`, `bind()`, `listen()`, `accept()`, `connect()`, `send()`/`recv()`, `sendto()`/`recvfrom()`, `shutdown()`, `getsockopt()`/`setsockopt()`
- **Nonblocking I/O** — `O_NONBLOCK` flag, `EAGAIN` returns, `select()`/`poll()`/`epoll()` integration
- **`SO_REUSEADDR`/`SO_REUSEPORT`** — allow multiple sockets on same port
- **`sendfile()`** — zero-copy file-to-socket transfer. File pages DMA'd directly to NIC without entering userspace.
- **Unix domain sockets** — full implementation (see IPC section)

### 7.5 Higher Protocols (Stretch)

- **DHCP client** — broadcast DISCOVER, receive OFFER, send REQUEST, receive ACK, configure interface
- **DNS resolver** — UDP queries to DNS server, A/AAAA record parsing. Caching. `/etc/resolv.conf` equivalent.
- **HTTP/1.1 server** — a minimal httpd as a userspace process. Serve files from VFS. This is a fantastic demo.
- **TLS** — integrate a small TLS library (BearSSL is tiny and readable) to add HTTPS support

---

## PHASE 8 — Months 8–9: Security Architecture

### 8.1 Access Control

- **Unix DAC** — user/group/other permission bits (rwxrwxrwx), `chmod`, `chown`, `umask`
- **Setuid/setgid** — executable runs with owner's privileges. `execve()` checks S_ISUID bit, sets effective UID.
- **Capabilities (POSIX)** — break root into fine-grained capabilities: `CAP_NET_ADMIN`, `CAP_SYS_PTRACE`, `CAP_KILL`, etc. Process can drop capabilities it doesn't need.
- **Mandatory Access Control (MAC) framework** — pluggable LSM (Linux Security Module) style hooks at all security-sensitive operations. Implement a simple policy engine on top.

### 8.2 Memory Security

- **SMEP (Supervisor Mode Execution Prevention)** — CPU refuses to execute userspace pages while in Ring 0. Set bit 20 in CR4. Blocks kernel exploits that redirect execution to user memory.
- **SMAP (Supervisor Mode Access Prevention)** — CPU refuses to read/write userspace pages without explicit `stac`/`clac` instructions. Set bit 21 in CR4. Forces explicit user memory access.
- **NX/XD bit** — mark data pages non-executable in page tables. Prevents shellcode execution on stack/heap.
- **KASLR** — randomize kernel load address at boot. Makes kernel address leaks less useful to attackers.
- **Stack canaries** — GCC `-fstack-protector` inserts random value below saved RIP. Checked on return; mismatch → panic.
- **Kernel address sanitizer (KASAN)** — shadow memory tracking every kernel heap byte. Catches out-of-bounds reads/writes and use-after-free. Expensive but invaluable for debugging.

### 8.3 Sandboxing & Isolation

- **`seccomp`** — per-process syscall filter. Process installs a BPF filter; kernel checks every syscall against it. Syscalls not on the whitelist → SIGKILL. Used by Chrome, Firefox, Docker.
- **Namespaces** — PID namespace (isolated process tree), mount namespace (isolated filesystem view), network namespace (isolated network stack — own interfaces, routing table, sockets), user namespace (unprivileged user maps to root inside namespace). Foundation for containers.
- **`chroot()` / `pivot_root()`** — change filesystem root for a process. `pivot_root` is the proper version used by container runtimes.
- **`ptrace()`** — allows one process to inspect/modify another's state. Used by debuggers. Implement `PTRACE_ATTACH`, `PTRACE_PEEKDATA`, `PTRACE_POKEDATA`, `PTRACE_SINGLESTEP`, `PTRACE_CONT`.

---

## PHASE 9 — Months 9–10: SMP (Multi-Core Support)

### 9.1 SMP Bootstrap

- **ACPI MADT parsing** — find all Local APIC entries = find all CPU cores
- **AP (Application Processor) startup** — BSP sends INIT-SIPI-SIPI IPI sequence to each AP. APs start in real mode at a known physical address (the trampoline), run through mode switches, initialize their own GDT/IDT/TSS, join the scheduler.
- **Per-CPU data** — each CPU has its own: current task pointer, kernel stack, GDT, TSS, APIC ID, scheduler run queue, softirq state, statistical counters. Access via `%gs` segment base (set per-CPU via MSR).
- **CPU hotplug** — bring CPUs online/offline at runtime. Migrate tasks away before offlining.

### 9.2 SMP Correctness

- **Lock auditing** — annotate every lock with the IRQ context it's safe to acquire in. Detect lock ordering violations that cause deadlock (lockdep-style).
- **Atomic operations** — `lock` prefix on x86 RMW instructions. `atomic_t`, `atomic_add()`, `atomic_cmpxchg()`, `atomic_fetch_or()` etc.
- **RCU (Read-Copy-Update)** — lock-free reads of shared data structures. Writers make a copy, update it, atomically swap pointer, wait for all readers to finish a quiescent period (scheduler tick), then free old copy. Critical for high-performance SMP.
- **Memory ordering** — understand and explicitly use `smp_mb()` (full barrier), `smp_rmb()` (read barrier), `smp_wmb()` (write barrier) at SMP synchronization points.
- **False sharing prevention** — pad hot per-CPU structs to cache line boundaries (`__cacheline_aligned`). Eliminates cache line ping-pong between cores.

---

## PHASE 10 — Months 10–11: Framebuffer GUI

### 10.1 Display Stack

- **Framebuffer driver** — linear framebuffer from GRUB/UEFI GOP or virtio-gpu. Write pixels at `fb_base + (y * pitch + x * bpp)`.
- **Double buffering** — render to back buffer, flip to front on vsync (or after each frame). Eliminates tearing.
- **PSF2 font rendering** — bitmap font, one glyph per character. Blit glyphs at correct position with alpha blending.
- **2D drawing primitives** — horizontal/vertical line (fast paths), arbitrary line (Bresenham), filled/outlined rectangle, circle, ellipse, triangle, polygon fill (scanline algorithm).
- **Alpha compositing** — Porter-Duff `OVER` operator for layered rendering: `result = src + dst*(1-src_alpha)`. SIMD-accelerated with SSE2.
- **Font rasterization** — integrate a small TTF rasterizer (stb_truetype is a single header file) for smooth fonts. This enables rendering any TrueType font.
- **Image decoding** — a small PNG decoder (libpng or stb_image) to load images from disk.

### 10.2 Window Manager

- **Window abstraction** — `struct window { x, y, width, height, z_order, framebuffer*, title }`.
- **Compositor** — maintain a z-ordered list of windows. Each window renders to its own buffer. Compositor blends them in order onto the screen framebuffer. Dirty region tracking to only re-composite what changed.
- **Window decoration** — title bar with close/minimize/maximize buttons. Render all decorations in the compositor, not the application (like Wayland's compositor-side decorations).
- **Input routing** — hit-test mouse position against window list (reverse z-order). Route keyboard events to focused window. Implement focus follows mouse and click-to-focus modes.
- **IPC protocol** — applications talk to the compositor over Unix domain sockets. Messages: create_window, destroy_window, map_window, resize_window, submit_buffer. The application and compositor share a memory region for the window framebuffer (zero-copy).
- **Damage protocol** — client marks dirty rectangles. Compositor only copies those regions. Big performance win.

### 10.3 Widget Toolkit

- **Basic widgets** — label, button (with hover/pressed states), text input, checkbox, radio button, scrollbar, list view, combo box
- **Layout engine** — absolute positioning, horizontal/vertical box layout, grid layout
- **Event model** — event objects (mouse click, key press, resize, expose) delivered to focused widget. Bubbling up the widget tree.
- **Terminal emulator** — implement VT100/ANSI escape code parsing (`\033[H`, `\033[2J`, color codes, cursor movement). Run your shell inside a GUI window. This makes the OS feel complete.

---

## PHASE 11 — Month 11–12: Userspace & POSIX Layer

### 11.1 C Standard Library (libc)

Building your own libc that compiles and links against your kernel:

- **String functions** — `memcpy`, `memmove`, `memset`, `strlen`, `strcpy`, `strcmp`, `strncmp`, `strdup`, `strcat`, optimized with SSE2 for large buffers
- **`printf` family** — full format string parser: `%d`, `%u`, `%x`, `%s`, `%f`, `%g`, `%p`, `%n`, width/precision/flags, `sprintf`, `snprintf`, `fprintf`, `dprintf`
- **`malloc`/`free`** — userspace heap allocator. Start with `dlmalloc` (Doug Lea's malloc). Calls `sbrk()`/`mmap()` for more memory. Later implement `jemalloc`-style thread-local arenas.
- **File I/O** — `FILE*` abstraction. `fopen`, `fclose`, `fread`, `fwrite`, `fgets`, `fputs`, `fprintf`, `fflush`, `fseek`, `ftell`. Buffered I/O (full, line, and unbuffered modes).
- **`setjmp`/`longjmp`** — save/restore register state for non-local jumps
- **Math library** — `sin`, `cos`, `tan`, `sqrt`, `pow`, `log`, `exp`. Use x87/SSE instructions.
- **`pthread` implementation** — maps to your kernel's thread `clone()` syscall. `pthread_mutex_t` → futex. `pthread_cond_t` → futex wait/wake. `pthread_once`, `pthread_key_create` (TLS).
- **`dlopen`/`dlsym`** — dynamic linking support. Load shared objects at runtime, resolve symbols.

### 11.2 Dynamic Linker

- **ELF shared object support** — `.so` files with `PIC` (Position Independent Code)
- **`ld.so`** — the dynamic linker/loader. Reads `PT_DYNAMIC` segment. Processes `DT_NEEDED` (required shared libs), `DT_RELA`/`DT_REL` (relocations), `DT_SYMTAB` (symbol table).
- **PLT/GOT** — Procedure Linkage Table / Global Offset Table. Lazy binding: first call to a shared function goes through PLT stub, linker resolves and patches GOT entry, subsequent calls go directly.
- **`LD_PRELOAD`** — inject a shared library before all others. Classic Unix mechanism for interposing on library calls.

### 11.3 POSIX Userspace Tools

Build these as actual userspace ELF binaries that run on your OS:

```
sh          — a real shell (parse commands, pipes, redirections, variables, builtins)
ls          — list directory contents with -l, -a, -h flags
cat         — print files, concatenate
cp / mv / rm / mkdir / rmdir / ln
echo / printf
grep        — regex pattern matching (implement a basic NFA-based regex engine)
find        — traverse directory tree with match criteria
ps          — read /proc, display process list
top         — dynamic process monitor, refresh every second
kill / killall
wc          — word/line/byte count
sort / uniq / head / tail / cut
hexdump     — binary file viewer
strace      — trace syscalls of a process via ptrace
vi          — a minimal text editor (just insert/normal mode and :wq is enough)
```

### 11.4 Init System

- **PID 1 (init)** — the first userspace process. Reads an inittab-style config. Spawns system services (networking daemon, VFS mount daemon, device manager).
- **Service supervision** — restart crashed services. Log stdout/stderr to files. Dependency ordering.
- **`/etc/fstab` equivalent** — mount filesystems at boot in order.

---

## PHASE 12 — Month 12: Hardening, Performance & Tooling

### 12.1 Observability

- **`ftrace` equivalent** — function call tracing via compiler instrumentation (`-pg` flag inserts `mcount()` calls). Toggle tracing per-function at runtime.
- **Performance counters** — use CPU PMU (Performance Monitoring Unit) via `rdpmc`. Count: cache misses, branch mispredictions, instructions retired, cycles. `perf stat` equivalent.
- **Kernel tracepoints** — static instrumentation at key points (context switch, page fault, syscall entry/exit, block I/O). Low overhead when disabled (single `nop` instruction, patched to a call when enabled).
- **`/proc/klog`** — kernel ring buffer with log levels (KERN_EMERG through KERN_DEBUG). `dmesg` userspace tool reads it.
- **GDB stub** — built-in kernel GDB remote protocol stub. Connect from host GDB without QEMU's `-s` flag. Lets you debug a running kernel from inside.

### 12.2 Testing Infrastructure

- **Unit tests for kernel subsystems** — memory allocator stress tests, scheduler correctness tests, VFS tests. Run as kernel threads on boot in test mode.
- **Syscall fuzzer** — generate random valid-ish syscall sequences, check for crashes/hangs. Like Trinity/syzkaller but tiny.
- **Heap corruption detector** — KASAN-style shadow memory for kernel heap. Every `kmalloc` gets a shadow byte. Out-of-bounds writes detected immediately.
- **Lock validator** — record lock acquisition order at runtime. Detect potential deadlock cycles (A→B and B→A ever acquired together?). Like Linux's lockdep.
- **CI with QEMU** — write a script that builds the kernel and runs QEMU headlessly. Check for successful boot, run test suite, verify exit code. Run on every commit.

### 12.3 Performance Tuning

- **`perf` profiling** — sample the instruction pointer on timer interrupt. Build a histogram. Find hot functions.
- **SIMD optimization** — `memcpy`, `memset`, checksums (IPv4, TCP), encryption with SSE2/AVX2 intrinsics
- **Huge pages (2MB)** — map kernel text/data with 2MB pages instead of 4KB. Massively reduces TLB pressure (fewer entries needed). Transparent huge pages for userspace too.
- **Lock-free data structures** — replace heavily-contended locks with lock-free alternatives using `cmpxchg`. Lock-free FIFO queue for the network RX path.
- **NUMA-aware allocation** — allocate memory from the NUMA node closest to the CPU running the allocating thread.

---

## Final Architecture Diagram

```
┌─────────────────────────────────────────────────────────┐
│                    USERSPACE                             │
│  shell  ls  grep  vi  httpd  ssh  your-app              │
│  ──────────────────────────────────────────             │
│  libc (malloc, printf, pthreads, sockets)               │
│  ld.so (dynamic linker)                                  │
├─────────────────────────────────────────────────────────┤
│              SYSCALL INTERFACE (64 calls)                │
├──────────────┬──────────────┬───────────────────────────┤
│   PROCESS    │    MEMORY    │         VFS               │
│  scheduler   │  PMM + VMM   │  ext2 tmpfs procfs sysfs  │
│  (CFS+RT)    │  slab+vmalloc│  dentry/inode cache       │
│  signals     │  demand page │  page cache               │
│  namespaces  │  CoW + ASLR  │  block layer + I/O sched  │
├──────────────┴──────────────┴───────────────────────────┤
│                   IPC LAYER                              │
│     pipes  sockets  shmem  futex  capabilities          │
├─────────────────────────────────────────────────────────┤
│                NETWORK STACK                             │
│     TCP   UDP   ICMP   ARP   IPv4   IPv6                │
│     BSD socket API    epoll                             │
├─────────────────────────────────────────────────────────┤
│                  DRIVER MODEL                            │
│  PCI  USB  UART  PS/2  e1000/virtio  ATA/AHCI  GPU     │
├─────────────────────────────────────────────────────────┤
│                   HAL / x86_64                           │
│  GDT  IDT  APIC  TSS  MSR  HPET  CPUID  SMP            │
├─────────────────────────────────────────────────────────┤
│                  BOOTLOADER                              │
│  Stage1(MBR) → Stage2(ELF loader) → Kernel             │
└─────────────────────────────────────────────────────────┘
```

---

At the end of 12 months, if you complete even 70% of this, you will have built something comparable in scope to early Linux (0.99 era) or a modern hobbyist OS like SerenityOS. That's not an exaggeration. Want me to generate the initial project scaffold — Makefile, linker script, boot entry, and kernel `main.c`?