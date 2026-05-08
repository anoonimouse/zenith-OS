#include "pmm.h"
#include "../hal/io.h"

static uint8_t* pmm_bitmap;
static uint64_t pmm_total_pages;
static uint64_t pmm_bitmap_size;

void pmm_init(uint64_t mem_size, uint64_t bitmap_addr) {
    pmm_total_pages = mem_size / PAGE_SIZE;
    pmm_bitmap = (uint8_t*)bitmap_addr;
    pmm_bitmap_size = pmm_total_pages / 8;

    // Initially mark everything as reserved (1)
    for (uint64_t i = 0; i < pmm_bitmap_size; i++) {
        pmm_bitmap[i] = 0xFF;
    }
    
    serial_write("PMM Initialized with ");
    // Convert to hex or just print message
    serial_write("bitmap.\n");
}

void pmm_mark_available(uint64_t addr, uint64_t len) {
    uint64_t start_page = addr / PAGE_SIZE;
    uint64_t page_count = len / PAGE_SIZE;

    for (uint64_t i = 0; i < page_count; i++) {
        uint64_t page = start_page + i;
        if (page < pmm_total_pages) {
            pmm_bitmap[page / 8] &= ~(1 << (page % 8));
        }
    }
}

void pmm_mark_reserved(uint64_t addr, uint64_t len) {
    uint64_t start_page = addr / PAGE_SIZE;
    uint64_t page_count = (len + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint64_t i = 0; i < page_count; i++) {
        uint64_t page = start_page + i;
        if (page < pmm_total_pages) {
            pmm_bitmap[page / 8] |= (1 << (page % 8));
        }
    }
}

void* pmm_alloc_page() {
    for (uint64_t i = 0; i < pmm_bitmap_size; i++) {
        if (pmm_bitmap[i] != 0xFF) {
            for (int j = 0; j < 8; j++) {
                if (!(pmm_bitmap[i] & (1 << j))) {
                    uint64_t page = i * 8 + j;
                    pmm_bitmap[i] |= (1 << j);
                    return (void*)(page * PAGE_SIZE);
                }
            }
        }
    }
    return NULL; // Out of memory
}

void pmm_free_page(void* addr) {
    uint64_t page = (uint64_t)addr / PAGE_SIZE;
    if (page < pmm_total_pages) {
        pmm_bitmap[page / 8] &= ~(1 << (page % 8));
    }
}
