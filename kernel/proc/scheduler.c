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
    void* kstack = pmm_alloc_page();
    current_task->stack_base = (uint64_t)kstack;
    current_task->next = current_task;
    task_list = current_task;
    
    serial_printf("Scheduler: Initialized (Main Task ID: %d)\n", current_task->id);
}

// Shim to jump to user mode from a kernel thread context
static void user_task_shim(void) {
    asm volatile("cli");
    serial_printf("Scheduler: Task %d jumping to Ring 3 at %p...\n", current_task->id, current_task->user_entry);
    asm volatile("sti");
    jump_to_user(current_task->user_entry, current_task->user_stack);
}

struct task* task_create_user(uint64_t entry, uint64_t stack) {
    struct task* new_task = (struct task*)pmm_alloc_page();
    // Ensure task structure is zeroed
    uint8_t* task_ptr = (uint8_t*)new_task;
    for (int i = 0; i < PAGE_SIZE; i++) task_ptr[i] = 0;

    void* kstack = pmm_alloc_page();
    
    new_task->id = next_task_id++;
    new_task->state = TASK_READY;
    new_task->stack_base = (uint64_t)kstack;
    new_task->user_entry = entry;
    new_task->user_stack = stack;
    
    // Create isolated address space
    new_task->address_space = vmm_create_address_space();
    
    // Initial kernel stack frame compatible with ISR exit
    struct interrupt_frame* frame = (struct interrupt_frame*)((uint8_t*)kstack + PAGE_SIZE - sizeof(struct interrupt_frame));
    
    // Zero out the frame
    uint8_t* p = (uint8_t*)frame;
    for (uint32_t i = 0; i < sizeof(struct interrupt_frame); i++) p[i] = 0;
    
    frame->rip = (uint64_t)user_task_shim;
    frame->cs = 0x08;        // Kernel Code
    frame->rflags = 0x202;  // IF enabled
    frame->rsp = (uint64_t)frame + sizeof(struct interrupt_frame); // Not really used for kernel return but good for alignment
    frame->ss = 0x10;        // Kernel Data
    
    new_task->rsp = (uint64_t)frame;
    
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
    asm volatile("int $0x81");
}

int task_get_current_id(void) {
    return current_task ? current_task->id : -1;
}

uint64_t scheduler_schedule(struct interrupt_frame* frame) {
    if (!current_task) return (uint64_t)frame;

    struct task* prev = current_task;
    struct task* next = current_task->next;
    
    // Find next ready task
    while (next->state != TASK_READY && next->state != TASK_RUNNING) {
        next = next->next;
        if (next == prev) break;
    }
    
    if (next == prev) {
        return (uint64_t)frame;
    }

    // If the frame is on the IST stack (task_switch_stack), we MUST copy it 
    // to the task's own kernel stack. Otherwise, the next interrupt will
    // overwrite it because IST resets the stack pointer.
    uint64_t frame_addr = (uint64_t)frame;
    // The task_switch_stack is 4096 bytes. We need to find its address.
    // In gdt.c it was static. Let's assume for now we can just copy if it's not
    // already on the task's kernel stack.
    if (frame_addr < prev->stack_base || frame_addr >= prev->stack_base + PAGE_SIZE) {
        // It's on some other stack (likely IST). Copy it to the top of the task's kernel stack.
        struct interrupt_frame* new_frame = (struct interrupt_frame*)(prev->stack_base + PAGE_SIZE - sizeof(struct interrupt_frame));
        uint8_t* src = (uint8_t*)frame;
        uint8_t* dst = (uint8_t*)new_frame;
        for (uint32_t i = 0; i < sizeof(struct interrupt_frame); i++) dst[i] = src[i];
        frame = new_frame;
    }

    // Switch context
    prev->rsp = (uint64_t)frame;
    prev->state = TASK_READY;

    current_task = next;
    current_task->state = TASK_RUNNING;

    if (current_task->address_space != prev->address_space) {
        vmm_switch_address_space(current_task->address_space);
    }
    
    if (current_task->stack_base) {
        gdt_set_kernel_stack(current_task->stack_base + PAGE_SIZE);
    }
    
    return current_task->rsp;
}
