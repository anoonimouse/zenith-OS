#include "apic.h"
#include "io.h"

#define MSR_APIC_BASE 0x1B

static uint64_t lapic_base = 0;



uint32_t lapic_read(uint32_t reg) {
    return *(volatile uint32_t*)(lapic_base + reg);
}

void lapic_write(uint32_t reg, uint32_t data) {
    *(volatile uint32_t*)(lapic_base + reg) = data;
}

void lapic_init(void) {
    uint64_t base_msr = rdmsr(MSR_APIC_BASE);
    lapic_base = base_msr & 0xFFFFF000;

    serial_printf("APIC: Local APIC base at %p\n", lapic_base);

    // Enable Local APIC by setting bit 8 of the Spurious Interrupt Vector Register
    // and provide a spurious vector (usually 0xFF)
    lapic_write(LAPIC_SVR, lapic_read(LAPIC_SVR) | LAPIC_SVR_ENABLE | 0xFF);

    serial_write("APIC: Local APIC enabled.\n");
}

void lapic_eoi(void) {
    lapic_write(LAPIC_EOI, 0);
}

void apic_timer_init(uint32_t frequency) {
    // Vector 32 (IRQ 0) for the timer, Periodic mode (bit 17)
    lapic_write(LAPIC_LVT_TMR, 32 | (1 << 17));
    
    // Divide configuration register (0 = divide by 2)
    lapic_write(LAPIC_TMRDIV, 0);
    
    // Initial count
    lapic_write(LAPIC_TMRINIT, frequency);
}
