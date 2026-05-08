#include "heap.h"
#include "pmm.h"
#include "../hal/io.h"

typedef struct heap_block {
    uint64_t size;
    struct heap_block* next;
    int free;
} heap_block_t;

#define HEAP_MIN_BLOCK_SIZE sizeof(heap_block_t)
static heap_block_t* heap_start = NULL;

void heap_init(void) {
    // Allocate initial 128KB for the heap
    void* initial_page = pmm_alloc_page();
    for (int i = 0; i < 31; i++) pmm_alloc_page(); // 32 pages = 128KB
    
    heap_start = (heap_block_t*)initial_page;
    heap_start->size = 32 * PAGE_SIZE - sizeof(heap_block_t);
    heap_start->next = NULL;
    heap_start->free = 1;

    serial_printf("Heap: Initialized with 128KB at %p\n", heap_start);
}

void* kmalloc(uint64_t size) {
    // Alignment to 16 bytes
    size = (size + 15) & ~15;

    heap_block_t* current = heap_start;
    while (current) {
        if (current->free && current->size >= size) {
            // Can we split this block?
            if (current->size >= size + sizeof(heap_block_t) + 16) {
                heap_block_t* new_block = (heap_block_t*)((uint8_t*)current + sizeof(heap_block_t) + size);
                new_block->size = current->size - size - sizeof(heap_block_t);
                new_block->free = 1;
                new_block->next = current->next;
                
                current->size = size;
                current->next = new_block;
            }
            current->free = 0;
            return (void*)((uint8_t*)current + sizeof(heap_block_t));
        }
        current = current->next;
    }

    // TODO: Extend heap by calling pmm_alloc_page
    serial_printf("Heap Error: Out of memory! Requested %d bytes\n", size);
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr) return;

    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
    block->free = 1;

    // Coalesce adjacent free blocks
    heap_block_t* current = heap_start;
    while (current && current->next) {
        if (current->free && current->next->free) {
            current->size += current->next->size + sizeof(heap_block_t);
            current->next = current->next->next;
            continue; // Check again for next block
        }
        current = current->next;
    }
}
