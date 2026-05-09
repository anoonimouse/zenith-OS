[BITS 64]

section .text

global jump_to_user
jump_to_user:
    ; Parameters: rdi = entry point, rsi = stack pointer
    
    ; 1. Save the current kernel GS_BASE before we mess with segment registers
    mov ecx, 0xC0000101 ; MSR_GS_BASE
    rdmsr               ; Read current GS_BASE (context) into EDX:EAX
    mov r8d, eax        ; Save low 32 bits
    mov r9d, edx        ; Save high 32 bits
    
    ; 2. Setup user segment registers
    ; GDT Selectors (from gdt.c):
    ; 0x18: User Data (Index 3) -> RPL 3: 0x1B
    ; 0x20: User Code (Index 4) -> RPL 3: 0x23
    
    mov ax, 0x1B        ; User Data Selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    
    ; Setting GS selector reloads the base from GDT (usually 0)
    xor ax, ax
    mov gs, ax
    
    ; 3. Restore the kernel context pointer to MSR_KERNEL_GS_BASE
    ; This makes it available for 'swapgs' in syscall_entry.asm
    mov eax, r8d
    mov edx, r9d
    mov ecx, 0xC0000102 ; MSR_KERNEL_GS_BASE
    wrmsr
    
    ; 4. Push stack frame for iretq
    push 0x1B           ; SS (User Data)
    push rsi            ; RSP (User Stack)
    push 0x202          ; RFLAGS (Interrupts enabled)
    push 0x23           ; CS (User Code)
    push rdi            ; RIP (User Entry)
    
    iretq
