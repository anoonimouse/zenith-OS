[BITS 64]

section .user_code

global user_program_start
global user_program_end

user_program_start:
    ; We are in Ring 3!
    
    ; System Call 1 (Write)
    mov rax, 1          ; SYS_WRITE
    mov rdi, 1          ; fd (stdout/serial)
    lea rsi, [rel msg]  ; buffer
    mov rdx, 46         ; count
    syscall
    
    ; Infinite loop
.loop:
    jmp .loop

msg: db "Hello from Ring 3! System calls are working.", 10, 0

user_program_end:
