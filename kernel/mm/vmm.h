#ifndef VMM_H
#define VMM_H

#include "../types.h"

#define PAGE_SIZE 4096

// Page Table Entry Flags
#define PTE_PRESENT  (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_USER     (1ULL << 2)
#define PTE_NX       (1ULL << 63)

void vmm_init(void);
void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);
uint64_t vmm_get_phys(uint64_t virt);
void vmm_dump_entry(uint64_t virt);

// Address Space Management
uint64_t vmm_create_address_space(void);
void vmm_switch_address_space(uint64_t pml4_phys);
uint64_t vmm_get_current_pml4(void);

#endif
