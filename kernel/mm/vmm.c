#include "vmm.h"
#include "pmm.h"
#include "../hal/io.h"

#define PAGE_ENTRIES 512

static uint64_t current_pml4_phys;

static uint64_t get_index(uint64_t virt, int level) {
    switch (level) {
        case 4: return (virt >> 39) & 0x1FF;
        case 3: return (virt >> 30) & 0x1FF;
        case 2: return (virt >> 21) & 0x1FF;
        case 1: return (virt >> 12) & 0x1FF;
        default: return 0;
    }
}

static inline uint64_t get_cr3() {
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

void vmm_init(void) {
    asm volatile("mov %%cr3, %0" : "=r"(current_pml4_phys));
    serial_printf("VMM: Initialized with PML4 at %p\n", current_pml4_phys);
}

uint64_t vmm_get_current_pml4(void) {
    return current_pml4_phys;
}

void vmm_switch_address_space(uint64_t pml4_phys) {
    current_pml4_phys = pml4_phys;
    asm volatile("mov %0, %%cr3" :: "r"(pml4_phys) : "memory");
}

uint64_t vmm_create_address_space(void) {
    uint64_t* new_pml4 = (uint64_t*)pmm_alloc_page();
    if (!new_pml4) return 0;

    // Zero out the new PML4
    for (int i = 0; i < PAGE_ENTRIES; i++) {
        new_pml4[i] = 0;
    }

    // Clone the kernel part of the address space.
    // In our identity map, we'll just copy the first 256 entries for now.
    // This is a simplification.
    uint64_t* current_v = (uint64_t*)current_pml4_phys;
    for (int i = 0; i < 256; i++) {
        new_pml4[i] = current_v[i];
    }

    return (uint64_t)new_pml4;
}

void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t* table = (uint64_t*)current_pml4_phys;

    for (int level = 4; level > 1; level--) {
        uint64_t index = get_index(virt, level);
        if (!(table[index] & PTE_PRESENT)) {
            uint64_t next_table_phys = (uint64_t)pmm_alloc_page();
            if (!next_table_phys) {
                serial_printf("VMM Error: Failed to allocate page table level %d\n", level - 1);
                return;
            }
            
            uint64_t* next_table_virt = (uint64_t*)next_table_phys;
            for (int i = 0; i < PAGE_ENTRIES; i++) {
                next_table_virt[i] = 0;
            }

            // If we are mapping a user page, the parent tables must also have the USER flag.
            uint64_t table_flags = PTE_PRESENT | PTE_WRITABLE;
            if (flags & PTE_USER) table_flags |= PTE_USER;
            
            table[index] = next_table_phys | table_flags;
        } else {
            // Propagate USER flag to parent tables if necessary
            if (flags & PTE_USER) {
                table[index] |= PTE_USER;
            }
        }
        table = (uint64_t*)(table[index] & ~0xFFFULL);
    }

    uint64_t index = get_index(virt, 1);
    table[index] = (phys & ~0xFFFULL) | flags | PTE_PRESENT;
    
    // Invalidate TLB for this address
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

void vmm_dump_entry(uint64_t virt) {
    uint64_t cr3 = get_cr3();
    serial_printf("VMM DUMP for %p (CR3: %p)\n", virt, cr3);

    uint64_t* pml4 = (uint64_t*)(cr3 & ~0xFFFULL);
    uint64_t i4 = get_index(virt, 4);
    uint64_t e4 = pml4[i4];
    serial_printf("  L4[%d]: %p (Flags: %x)\n", i4, e4 & ~0xFFFULL, e4 & 0xFFF);

    if (!(e4 & PTE_PRESENT)) return;
    
    uint64_t* pdpt = (uint64_t*)(e4 & ~0xFFFULL);
    uint64_t i3 = get_index(virt, 3);
    uint64_t e3 = pdpt[i3];
    serial_printf("  L3[%d]: %p (Flags: %x)\n", i3, e3 & ~0xFFFULL, e3 & 0xFFF);

    if (!(e3 & PTE_PRESENT) || (e3 & (1 << 7))) return; // Huge page check

    uint64_t* pd = (uint64_t*)(e3 & ~0xFFFULL);
    uint64_t i2 = get_index(virt, 2);
    uint64_t e2 = pd[i2];
    serial_printf("  L2[%d]: %p (Flags: %x)\n", i2, e2 & ~0xFFFULL, e2 & 0xFFF);

    if (!(e2 & PTE_PRESENT) || (e2 & (1 << 7))) return;

    uint64_t* pt = (uint64_t*)(e2 & ~0xFFFULL);
    uint64_t i1 = get_index(virt, 1);
    uint64_t e1 = pt[i1];
    serial_printf("  L1[%d]: %p (Flags: %x)\n", i1, e1 & ~0xFFFULL, e1 & 0xFFF);
}
