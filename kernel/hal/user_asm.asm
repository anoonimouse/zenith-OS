[BITS 64]

section .text

global jump_to_user
jump_to_user:
    ; Parameters: rdi = entry point, rsi = stack pointer
    
    ; GDT Selectors (from gdt.c):
    ; 0x18: User Data (Index 3) -> RPL 3: 0x1B
    ; 0x20: User Code (Index 4) -> RPL 3: 0x23
    
    mov ax, 0x1B        ; User Data Selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Push stack frame for iretq
    push 0x1B           ; SS
    push rsi            ; RSP
    push 0x202          ; RFLAGS (Interrupts enabled)
    push 0x23           ; CS
    push rdi            ; RIP
    
    iretq
