#include "keyboard.h"
#include "io.h"

#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64

static const char scancode_ascii[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

static char kbd_buffer[256];
static int kbd_head = 0;
static int kbd_tail = 0;

void keyboard_init(void) {
    serial_printf("Keyboard: Initialized\n");
}

void keyboard_handler(void) {
    uint8_t status = inb(KBD_STATUS_PORT);
    if (status & 0x01) {
        uint8_t scancode = inb(KBD_DATA_PORT);
        
        // Only handle key press (ignore break codes for now)
        if (scancode < 0x80) {
            char c = 0;
            if (scancode < sizeof(scancode_ascii)) {
                c = scancode_ascii[scancode];
            }
            
            if (c) {
                // Add to buffer
                int next = (kbd_head + 1) % 256;
                if (next != kbd_tail) {
                    kbd_buffer[kbd_head] = c;
                    kbd_head = next;
                }
                
                // Echo to serial for debugging
                serial_printf("KBD: %c\n", c);
            }
        }
    }
}

char keyboard_get_char(void) {
    if (kbd_head == kbd_tail) return 0;
    
    char c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % 256;
    return c;
}
