#include "gdt.h"
#include "io.h"

static struct tss kernel_tss;
static uint8_t gdt_data[5 * 8 + 16];
static struct gdt_ptr gdt_ptr;

static struct cpu_context current_cpu_context __attribute__((aligned(16)));

// Safe stack for double faults
static uint8_t double_fault_stack[4096] __attribute__((aligned(16)));
// Dedicated stack for task switching/scheduling to ensure consistent frame
static uint8_t task_switch_stack[4096] __attribute__((aligned(16)));

extern void gdt_load(struct gdt_ptr* ptr);
extern void tss_load();

#define MSR_GS_BASE 0xC0000101
#define MSR_KERNEL_GS_BASE 0xC0000102

void gdt_init() {
    struct gdt_entry* gdt = (struct gdt_entry*)gdt_data;

    // Null
    gdt[0].limit_low = 0; gdt[0].base_low = 0; gdt[0].base_middle = 0;
    gdt[0].access = 0; gdt[0].granularity = 0; gdt[0].base_high = 0;

    // Kernel Code
    gdt[1].limit_low = 0; gdt[1].base_low = 0; gdt[1].base_middle = 0;
    gdt[1].access = 0x9A; gdt[1].granularity = 0x20; gdt[1].base_high = 0;

    // Kernel Data
    gdt[2].limit_low = 0; gdt[2].base_low = 0; gdt[2].base_middle = 0;
    gdt[2].access = 0x92; gdt[2].granularity = 0; gdt[2].base_high = 0;

    // User Data
    gdt[3].limit_low = 0; gdt[3].base_low = 0; gdt[3].base_middle = 0;
    gdt[3].access = 0xF2; gdt[3].granularity = 0; gdt[3].base_high = 0;

    // User Code
    gdt[4].limit_low = 0; gdt[4].base_low = 0; gdt[4].base_middle = 0;
    gdt[4].access = 0xFA; gdt[4].granularity = 0x20; gdt[4].base_high = 0;

    // TSS Descriptor (at index 5, takes 16 bytes)
    uint64_t tss_base = (uint64_t)&kernel_tss;
    uint32_t tss_limit = sizeof(struct tss) - 1;
    struct tss_descriptor* tss = (struct tss_descriptor*)&gdt_data[5 * 8];

    tss->limit_low = tss_limit & 0xFFFF;
    tss->base_low = tss_base & 0xFFFF;
    tss->base_lower_middle = (tss_base >> 16) & 0xFF;
    tss->access = 0x89; // Present, Ring 0, TSS
    tss->granularity = 0;
    tss->base_upper_middle = (tss_base >> 24) & 0xFF;
    tss->base_high = (tss_base >> 32) & 0xFFFFFFFF;
    tss->reserved = 0;

    // Setup TSS
    for (uint32_t i = 0; i < (uint32_t)sizeof(struct tss); i++) {
        ((uint8_t*)&kernel_tss)[i] = 0;
    }
    
    // IST1 for Double Fault
    kernel_tss.ist1 = (uint64_t)&double_fault_stack[4096];
    // IST2 for Task Switching/Scheduling
    kernel_tss.ist2 = (uint64_t)&task_switch_stack[4096];
    kernel_tss.iopb_offset = sizeof(struct tss);

    // Initialize GDT pointer
    gdt_ptr.limit = sizeof(gdt_data) - 1;
    gdt_ptr.base = (uint64_t)gdt_data;

    // Load GDT
    gdt_load(&gdt_ptr);
    
    // Load TSS
    tss_load();

    // Setup GS_BASE and KERNEL_GS_BASE for syscall stack switching
    wrmsr(MSR_GS_BASE, (uint64_t)&current_cpu_context);
    wrmsr(MSR_KERNEL_GS_BASE, (uint64_t)&current_cpu_context);

    serial_printf("GDT: Initialized with TSS at %p and GS_BASE at %p\n", &kernel_tss, &current_cpu_context);
}

void gdt_set_kernel_stack(uint64_t stack) {
    kernel_tss.rsp0 = stack;
    current_cpu_context.kernel_stack = stack;
}
