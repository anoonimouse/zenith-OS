#include "syscall.h"
#include "task.h"
#include "../hal/io.h"

extern void syscall_entry(void);



#define MSR_STAR     0xC0000081
#define MSR_LSTAR    0xC0000082
#define MSR_CSTAR    0xC0000083 // Not used in long mode
#define MSR_SFMASK   0xC0000084
#define MSR_EFER     0xC0000080

void syscall_init(void) {
    // Enable syscall/sysret in EFER
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= 1; // SCE (System Call Enable)
    wrmsr(MSR_EFER, efer);

    // Set up STAR MSR
    // Bits 47:32: Kernel CS/SS base (0x08)
    // Bits 63:48: User CS/SS base (0x10)
    uint64_t star = ((uint64_t)0x10 << 48) | ((uint64_t)0x08 << 32);
    wrmsr(MSR_STAR, star);

    // Set up LSTAR MSR (entry point)
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    // Set up SFMASK (flags to clear on syscall)
    // We want to disable interrupts on syscall entry
    wrmsr(MSR_SFMASK, 0x200); // Clear IF (bit 9)

    serial_printf("Syscall: Initialized at %p\n", syscall_entry);
}

typedef void (*syscall_handler_t)(struct syscall_regs*);

void sys_write(struct syscall_regs* regs) {
    // rdi = fd, rsi = buf, rdx = count
    serial_write((const char*)regs->rsi);
    regs->rax = regs->rdx;
}
 
void sys_yield(struct syscall_regs* regs) {
    (void)regs;
    task_yield();
}

void sys_getpid(struct syscall_regs* regs) {
    regs->rax = task_get_current_id();
}

void sys_print_hex(struct syscall_regs* regs) {
    serial_printf("%p\n", regs->rdi);
}

void sys_exit(struct syscall_regs* regs) {
    serial_printf("Task exiting with code %d\n", regs->rdi);
    // For now, just hang or yield indefinitely
    // We'll implement proper task destruction later
    while(1) {
        asm volatile("hlt");
    }
}

static syscall_handler_t syscall_table[] = {
    [SYS_WRITE]     = sys_write,
    [SYS_PRINT_HEX] = sys_print_hex,
    [SYS_YIELD]     = sys_yield,
    [SYS_GETPID]    = sys_getpid,
    [SYS_EXIT]      = sys_exit,
};

#define MAX_SYSCALL (sizeof(syscall_table) / sizeof(syscall_handler_t))

void syscall_dispatcher(struct syscall_regs* regs) {
    uint64_t num = regs->rax;

    if (num < MAX_SYSCALL && syscall_table[num]) {
        syscall_table[num](regs);
    } else {
        serial_printf("Syscall: Unknown syscall %d\n", num);
        regs->rax = -1;
    }
}
