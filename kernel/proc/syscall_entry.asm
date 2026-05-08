[bits 64]

extern syscall_dispatcher
global syscall_entry

syscall_entry:
    ; syscall saves RIP in RCX and RFLAGS in R11
    ; It also switches CS/SS based on STAR MSR

    ; For now, we are in Ring 0, so we use the same stack.
    ; In a real OS, we would swapgs and switch to a kernel stack.

    ; Save registers (similar to interrupt frame, but RCX and R11 are already used by syscall)
    push r11 ; saved rflags
    push rcx ; saved rip
    
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

    ; Set up arguments for syscall_dispatcher(num, arg1, arg2, arg3, arg4, arg5)
    ; In x64 ABI: rdi, rsi, rdx, rcx, r8, r9
    ; Syscall convention (Linux-like): rax=num, rdi, rsi, rdx, r10, r8, r9
    
    mov r9, r9
    mov r8, r8
    mov rcx, r10 ; syscall uses r10 instead of rcx for 4th arg because rcx is used for rip
    mov rdx, rdx
    mov rsi, rsi
    mov rdi, rdi
    mov rax, rax ; syscall number

    ; Note: we need to pass rax as the first argument to our C dispatcher
    ; Let's adjust the calling convention for our dispatcher:
    ; void syscall_dispatcher(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5)
    
    ; Push current registers as arguments
    mov r9, r9
    mov r8, r8
    mov rcx, r10
    mov rdx, rdx
    mov rsi, rdi
    mov rdi, rax ; First arg is syscall number
    
    ; Wait, the Linux syscall convention is:
    ; rax: syscall number
    ; args: rdi, rsi, rdx, r10, r8, r9
    
    ; Let's just pass a pointer to the saved registers to the dispatcher
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
    
    pop rcx ; restore rip
    pop r11 ; restore rflags

    ; Return to Ring 3 using sysret
    o64 sysret
