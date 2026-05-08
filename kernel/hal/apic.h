#ifndef APIC_H
#define APIC_H

#include "../types.h"

// APIC Register Offsets
#define LAPIC_ID            0x0020
#define LAPIC_VER           0x0030
#define LAPIC_TPR           0x0080
#define LAPIC_EOI           0x00B0
#define LAPIC_LDR           0x00D0
#define LAPIC_DFR           0x00E0
#define LAPIC_SVR           0x00F0
#define LAPIC_ESR           0x0280
#define LAPIC_ICR_LOW       0x0300
#define LAPIC_ICR_HIGH      0x0310
#define LAPIC_LVT_TMR       0x0320
#define LAPIC_LVT_PERF      0x0340
#define LAPIC_LVT_LINT0     0x0350
#define LAPIC_LVT_LINT1     0x0360
#define LAPIC_LVT_ERR       0x0370
#define LAPIC_TMRINIT       0x0380
#define LAPIC_TMRCURR       0x0390
#define LAPIC_TMRDIV        0x03E0

#define LAPIC_SVR_ENABLE    0x100

void lapic_init(void);
void lapic_eoi(void);
void apic_timer_init(uint32_t frequency);
uint32_t lapic_read(uint32_t reg);
void lapic_write(uint32_t reg, uint32_t data);

#endif
