#include "timer.h"
#include "io.h"

void timer_init(uint32_t frequency) {
    // The PIT frequency is 1.193182 MHz
    uint32_t divisor = 1193182 / frequency;

    // Send the command byte (0x36) to the PIT command port (0x43)
    // 0x36 = 00110110: Channel 0, lobyte/hibyte, Mode 3, Binary
    outb(0x43, 0x36);

    // Send the divisor to Channel 0 data port (0x40)
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
    
    serial_printf("Timer: Initialized at %d Hz\n", frequency);
}
