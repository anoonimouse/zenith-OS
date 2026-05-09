[bits 64]

extern syscall_dispatcher

section .text
global syscall_entry

syscall_entry:
    ; syscall saves RIP in RCX and RFLAGS in R11
    ; It also switches CS/SS based on STAR MSR

    ; At this point:
    ; RCX = return RIP
    ; R11 = saved RFLAGS
    ; RSP = User RSP
    
    ; Switch to kernel stack using GS_BASE context
    swapgs
    mov [gs:8], rsp      ; Save user RSP in cpu_context.user_stack_temp
    mov rsp, [gs:0]      ; Load kernel RSP from cpu_context.kernel_stack
    
    ; Push user state for syscall_regs
    push r11 ; saved rflags
    push rcx ; saved rip
    push qword [gs:8] ; user rsp
    
    push rax
    push rbx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r12
    push r13
    push r14
    push r15

    ; Set up argument for syscall_dispatcher(struct syscall_regs*)
    mov rdi, rsp
    call syscall_dispatcher

    ; Restore registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rbx
    pop rax
    
    ; Restore user state in correct order
    pop rdi ; Temporarily pop user RSP into RDI
    pop rcx ; Restore return RIP
    pop r11 ; Restore RFLAGS
    
    mov [gs:8], rdi      ; Store user RSP back in GS for final restore
    mov rsp, [gs:8]      ; Restore user RSP
    swapgs
    
    ; Return to Ring 3 using sysret
    o64 sysret
