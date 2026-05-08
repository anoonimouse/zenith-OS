#include "task.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../hal/io.h"
#include "../hal/gdt.h"

static struct task* current_task;
static struct task* task_list;
static int next_task_id = 0;

extern void context_switch(uint64_t* old_rsp, uint64_t new_rsp);
extern void jump_to_user(uint64_t entry, uint64_t stack);

void task_init(void) {
    current_task = (struct task*)pmm_alloc_page();
    current_task->id = next_task_id++;
    current_task->state = TASK_RUNNING;
    current_task->address_space = vmm_get_current_pml4();
    current_task->next = current_task;
    task_list = current_task;
    
    serial_printf("Scheduler: Initialized (Main Task ID: %d)\n", current_task->id);
}

// Shim to jump to user mode from a kernel thread context
static void user_task_shim(void) {
    serial_printf("Scheduler: Task %d jumping to Ring 3...\n", current_task->id);
    jump_to_user(current_task->user_entry, current_task->user_stack);
}

struct task* task_create_user(uint64_t entry, uint64_t stack) {
    struct task* new_task = (struct task*)pmm_alloc_page();
    void* kstack = pmm_alloc_page();
    
    new_task->id = next_task_id++;
    new_task->state = TASK_READY;
    new_task->stack_base = (uint64_t)kstack;
    new_task->user_entry = entry;
    new_task->user_stack = stack;
    
    // Create isolated address space
    new_task->address_space = vmm_create_address_space();
    
    // Initial kernel stack frame for context switch
    uint64_t* stack_ptr = (uint64_t*)((uint8_t*)kstack + PAGE_SIZE);
    *(--stack_ptr) = (uint64_t)user_task_shim;
    for (int i = 0; i < 6; i++) *(--stack_ptr) = 0; // rbp, rbx, r12-r15
    new_task->rsp = (uint64_t)stack_ptr;
    
    new_task->next = task_list->next;
    task_list->next = new_task;
    
    return new_task;
}

void task_load_user_program(struct task* task, uint8_t* code_start, uint8_t* code_end) {
    uint64_t old_pml4 = vmm_get_current_pml4();
    vmm_switch_address_space(task->address_space);
    
    // Map and copy code
    uint64_t code_len = (uint64_t)code_end - (uint64_t)code_start;
    uint64_t pages = (code_len + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (uint64_t i = 0; i < pages; i++) {
        void* phys = pmm_alloc_page();
        vmm_map(task->user_entry + i * PAGE_SIZE, (uint64_t)phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    }
    
    for (uint64_t i = 0; i < code_len; i++) {
        ((uint8_t*)task->user_entry)[i] = code_start[i];
    }
    
    // Map stack (1 page for now)
    void* stack_phys = pmm_alloc_page();
    vmm_map(task->user_stack - PAGE_SIZE, (uint64_t)stack_phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    
    vmm_switch_address_space(old_pml4);
}

void task_yield(void) {
    struct task* prev = current_task;
    struct task* next = current_task->next;
    
    while (next->state != TASK_READY && next->state != TASK_RUNNING) {
        next = next->next;
        if (next == prev) return;
    }
    
    if (next == prev) return;

    current_task = next;
    current_task->state = TASK_RUNNING;
    prev->state = TASK_READY;
    
    // Switch address space if necessary
    if (next->address_space != prev->address_space) {
        vmm_switch_address_space(next->address_space);
    }
    
    // Update TSS kernel stack for user -> kernel transitions
    gdt_set_kernel_stack(next->stack_base + PAGE_SIZE);
    
    context_switch(&prev->rsp, next->rsp);
}
