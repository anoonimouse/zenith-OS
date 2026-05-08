[bits 64]

global context_switch
; void context_switch(uint64_t* old_rsp, uint64_t new_rsp)
context_switch:
    ; Save current task's registers
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; Save current RSP to old_rsp
    mov [rdi], rsp

    ; Load new RSP
    mov rsp, rsi

    ; Restore next task's registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ret
