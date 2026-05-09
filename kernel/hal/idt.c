#include "idt.h"
#include "io.h"

static struct idt_entry idt[256];
static struct idt_ptr idtp;

// Defined in assembly
extern void idt_flush(uint64_t);

void idt_set_gate(uint8_t vector, void* isr, uint8_t flags, uint8_t ist) {
    uint64_t addr = (uint64_t)isr;
    idt[vector].offset_low = addr & 0xFFFF;
    idt[vector].offset_mid = (addr >> 16) & 0xFFFF;
    idt[vector].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].selector = 0x08; // Kernel code segment (GDT)
    idt[vector].ist = ist;
    idt[vector].type_attr = flags;
    idt[vector].reserved = 0;
}

void idt_init(void) {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uint64_t)&idt;

    // Clear IDT
    for (int i = 0; i < 256; i++) {
        idt[i].type_attr = 0;
    }

    // Load IDT
    idt_flush((uint64_t)&idtp);

    // Register Exceptions
    idt_set_gate(0, isr0, IDT_ATTR_INTERRUPT, 0); idt_set_gate(1, isr1, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(2, isr2, IDT_ATTR_INTERRUPT, 0); idt_set_gate(3, isr3, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(4, isr4, IDT_ATTR_INTERRUPT, 0); idt_set_gate(5, isr5, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(6, isr6, IDT_ATTR_INTERRUPT, 0); idt_set_gate(7, isr7, IDT_ATTR_INTERRUPT, 0);
    
    // Double Fault gets IST 1
    idt_set_gate(8, isr8, IDT_ATTR_INTERRUPT, 1); 
    
    idt_set_gate(9, isr9, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(10, isr10, IDT_ATTR_INTERRUPT, 0); idt_set_gate(11, isr11, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(12, isr12, IDT_ATTR_INTERRUPT, 0); idt_set_gate(13, isr13, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(14, isr14, IDT_ATTR_INTERRUPT, 0); idt_set_gate(15, isr15, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(16, isr16, IDT_ATTR_INTERRUPT, 0); idt_set_gate(17, isr17, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(18, isr18, IDT_ATTR_INTERRUPT, 0); idt_set_gate(19, isr19, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(20, isr20, IDT_ATTR_INTERRUPT, 0); idt_set_gate(21, isr21, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(22, isr22, IDT_ATTR_INTERRUPT, 0); idt_set_gate(23, isr23, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(24, isr24, IDT_ATTR_INTERRUPT, 0); idt_set_gate(25, isr25, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(26, isr26, IDT_ATTR_INTERRUPT, 0); idt_set_gate(27, isr27, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(28, isr28, IDT_ATTR_INTERRUPT, 0); idt_set_gate(29, isr29, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(30, isr30, IDT_ATTR_INTERRUPT, 0); idt_set_gate(31, isr31, IDT_ATTR_INTERRUPT, 0);

    // Register IRQs
    idt_set_gate(32, irq0, IDT_ATTR_INTERRUPT, 2); idt_set_gate(33, irq1, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(34, irq2, IDT_ATTR_INTERRUPT, 0); idt_set_gate(35, irq3, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(36, irq4, IDT_ATTR_INTERRUPT, 0); idt_set_gate(37, irq5, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(38, irq6, IDT_ATTR_INTERRUPT, 0); idt_set_gate(39, irq7, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(40, irq8, IDT_ATTR_INTERRUPT, 0); idt_set_gate(41, irq9, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(42, irq10, IDT_ATTR_INTERRUPT, 0); idt_set_gate(43, irq11, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(44, irq12, IDT_ATTR_INTERRUPT, 0); idt_set_gate(45, irq13, IDT_ATTR_INTERRUPT, 0);
    idt_set_gate(46, irq14, IDT_ATTR_INTERRUPT, 0); idt_set_gate(47, irq15, IDT_ATTR_INTERRUPT, 0);

    extern void isr129();
    idt_set_gate(129, isr129, 0x8E, 2);

    serial_printf("IDT: Initialized and loaded at %p\n", &idt);
}
