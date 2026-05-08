#ifndef TASK_H
#define TASK_H

#include "../types.h"

#define KERNEL_STACK_SIZE 4096

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_TERMINATED
} task_state_t;

struct task {
    uint64_t rsp;           // Current stack pointer (top of saved context)
    uint64_t stack_base;    // Base of the stack
    task_state_t state;
    int id;
    
    // Ring 3 Support
    uint64_t address_space; // PML4 physical address
    uint64_t user_entry;
    uint64_t user_stack;
    
    struct task* next;
};

void task_init(void);
struct task* task_create(void (*entry)(void));
struct task* task_create_user(uint64_t entry, uint64_t stack);
void task_load_user_program(struct task* task, uint8_t* code_start, uint8_t* code_end);
void task_yield(void);

#endif
