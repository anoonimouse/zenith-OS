#include "types.h"
#include "hal/io.h"
#include "hal/apic.h"
#include "hal/keyboard.h"
#include "hal/gdt.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/heap.h"
#include "hal/idt.h"
#include "hal/timer.h"
#include "proc/task.h"
#include "proc/syscall.h"

// Symbols from user_test.asm
extern uint8_t user_program_start[];
extern uint8_t user_program_end[];

extern char __kernel_end[];

// Multiboot2 structures
struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

struct multiboot_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
};

struct multiboot_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct multiboot_mmap_entry entries[];
};

struct multiboot1_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
};

struct multiboot1_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));

void kmain(uint64_t multiboot_info_addr, uint64_t magic) {
    serial_write("TinyOS Kernel Started\n");

    // VGA text buffer starts at 0xB8000
    volatile char* video_memory = (volatile char*)0xB8000;
    for (int i = 0; i < 80 * 25 * 2; i += 2) {
        video_memory[i] = ' ';
        video_memory[i+1] = 0x07;
    }

    uint64_t total_mem = 0;
    uint64_t bitmap_addr = 0;
    uint32_t multiboot_size = 0;

    if (magic == 0x36d76289) { // Multiboot 2
        multiboot_size = *(uint32_t*)multiboot_info_addr;
        bitmap_addr = (multiboot_info_addr + multiboot_size + 4095) & ~4095;
        
        struct multiboot_tag* tag = (struct multiboot_tag*)(multiboot_info_addr + 8);
        struct multiboot_tag_mmap* mmap_tag = NULL;

        while (tag->type != 0) {
            if (tag->type == 6) {
                mmap_tag = (struct multiboot_tag_mmap*)tag;
                int entry_count = (mmap_tag->size - sizeof(struct multiboot_tag_mmap)) / mmap_tag->entry_size;
                for (int j = 0; j < entry_count; j++) {
                    uint64_t end = mmap_tag->entries[j].addr + mmap_tag->entries[j].len;
                    if (end > total_mem) total_mem = end;
                }
            }
            tag = (struct multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7));
        }

        if (!mmap_tag) {
            serial_write("Error: Multiboot2 Memory Map not found!\n");
            while(1) asm volatile("hlt");
        }

        pmm_init(total_mem, bitmap_addr);
        int entry_count = (mmap_tag->size - sizeof(struct multiboot_tag_mmap)) / mmap_tag->entry_size;
        for (int j = 0; j < entry_count; j++) {
            if (mmap_tag->entries[j].type == 1) {
                pmm_mark_available(mmap_tag->entries[j].addr, mmap_tag->entries[j].len);
            }
        }
        pmm_mark_reserved(multiboot_info_addr, multiboot_size);

    } else if (magic == 0x2badb002) { // Multiboot 1
        struct multiboot1_info* mb1 = (struct multiboot1_info*)multiboot_info_addr;
        
        // Find total memory from mmap
        struct multiboot1_mmap_entry* mmap = (struct multiboot1_mmap_entry*)(uint64_t)mb1->mmap_addr;
        uint32_t mmap_len = mb1->mmap_length;

        for (uint32_t i = 0; i < mmap_len; ) {
            struct multiboot1_mmap_entry* entry = (struct multiboot1_mmap_entry*)((uint8_t*)mmap + i);
            uint64_t end = entry->addr + entry->len;
            if (end > total_mem) total_mem = end;
            i += entry->size + 4;
        }

        bitmap_addr = ((uint64_t)__kernel_end + 4095) & ~4095;
        pmm_init(total_mem, bitmap_addr);

        for (uint32_t i = 0; i < mmap_len; ) {
            struct multiboot1_mmap_entry* entry = (struct multiboot1_mmap_entry*)((uint8_t*)mmap + i);
            if (entry->type == 1) {
                pmm_mark_available(entry->addr, entry->len);
            }
            i += entry->size + 4;
        }
        pmm_mark_reserved(multiboot_info_addr, sizeof(struct multiboot1_info));
    } else {
        serial_write("Error: Unknown bootloader magic!\n");
        while(1) asm volatile("hlt");
    }

    // Common reservations
    pmm_mark_reserved(0, 0x100000); // 1MB reserved
    pmm_mark_reserved(0x100000, (uint64_t)__kernel_end - 0x100000);
    pmm_mark_reserved(bitmap_addr, (total_mem / PAGE_SIZE) / 8);

    // Initialize Heap
    heap_init();

    // Initialize VMM
    vmm_init();

    // Map Local APIC (usually at 0xFEE00000)
    vmm_map(0xFEE00000, 0xFEE00000, PTE_PRESENT | PTE_WRITABLE);

    // Initialize CPU & Interrupts
    gdt_init();
    lapic_init();
    idt_init();
    pic_init();
    apic_timer_init(100);
    keyboard_init();
    syscall_init();

    // Initialize Scheduler
    task_init();

    serial_printf("Main: Creating user task...\n");
    struct task* utask = task_create_user(0x40000000, 0x40100000);
    task_load_user_program(utask, user_program_start, user_program_end);

    serial_printf("Diagnostic: Checking user mapping...\n");
    vmm_dump_entry(0x40000000);
    
    serial_printf("Main: Starting multitasking...\n");
    asm volatile("sti");
    task_yield();

    while (1) {
        asm volatile("hlt");
    }
}
