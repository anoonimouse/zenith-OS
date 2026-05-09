#ifndef SYSCALL_H
#define SYSCALL_H

#include "../types.h"

#define SYS_WRITE 1
#define SYS_PRINT_HEX 2
#define SYS_YIELD 24
#define SYS_GETPID 39
#define SYS_EXIT  60

void syscall_init(void);

struct syscall_regs {
    uint64_t r15, r14, r13, r12, r10, r9, r8, rdi, rsi, rbp, rdx, rbx, rax;
    uint64_t rsp;
    uint64_t rip, rflags;
};

#endif
