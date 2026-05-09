#include "io.h"
#include "idt.h"
#include "apic.h"
#include "keyboard.h"
#include "../mm/vmm.h"
#include "../proc/task.h"


static uint64_t timer_ticks = 0;

uint64_t interrupt_handler(struct interrupt_frame* frame) {
    if (frame->vector < 32) {
        serial_printf("CPU EXCEPTION %d (Error Code: %p) at RIP: %p\n", 
                      frame->vector, frame->error_code, frame->rip);
        serial_printf("  CS: %p  SS: %p  RSP: %p  RFLAGS: %p\n",
                      frame->cs, frame->ss, frame->rsp, frame->rflags);
        
        // If it's a page fault, print CR2
        if (frame->vector == 14) {
            uint64_t cr2;
            asm volatile("mov %%cr2, %0" : "=r"(cr2));
            serial_printf("  Page Fault at address: %p\n", cr2);
        }

        // Dump page table info for the RIP if it's a suspicious address
        if (frame->vector == 8 || frame->vector == 14 || frame->vector == 13) {
            vmm_dump_entry(frame->rip);
            vmm_dump_entry(frame->rsp);
        }

        while (1) { asm volatile("hlt"); }
    } else if (frame->vector >= 32 && frame->vector < 48) {
        // IRQ
        // Send EOI (End of Interrupt) to PIC
        if (frame->vector >= 40) {
            outb(0xA0, 0x20); // Send to slave PIC
        }
        outb(0x20, 0x20); // Send to master PIC

        if (frame->vector == 32) {
            timer_ticks++;
            lapic_eoi();
            return scheduler_schedule(frame);
        } else if (frame->vector == 33) {
            keyboard_handler();
            lapic_eoi();
        } else {
            lapic_eoi();
        }
    } else if (frame->vector == 129) {
        return scheduler_schedule(frame);
    } else {
        serial_printf("UNKNOWN INTERRUPT %d\n", frame->vector);
    }

    return (uint64_t)frame;
}

// Function to initialize PIC
void pic_init(void) {
    // ICW1
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    // ICW2 (Remap IRQ 0-15 to 32-47)
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    // ICW3
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    // ICW4
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    // Mask all interrupts except timer (IRQ0) and keyboard (IRQ1)
    outb(0x21, 0xFC);
    outb(0xA1, 0xFF);
    
    serial_write("PIC: Initialized and remapped IRQs to 32-47\n");
}
