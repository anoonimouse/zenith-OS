#ifndef PMM_H
#define PMM_H

#include "../types.h"



void pmm_init(uint64_t mem_size, uint64_t bitmap_addr);
void pmm_mark_available(uint64_t addr, uint64_t len);
void pmm_mark_reserved(uint64_t addr, uint64_t len);

void* pmm_alloc_page();
void pmm_free_page(void* addr);

#endif
