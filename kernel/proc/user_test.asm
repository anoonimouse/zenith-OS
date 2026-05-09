[BITS 64]

section .user_code

global user_program_start
global user_program_end

user_program_start:
    ; System Call 39 (GetPID)
    mov rax, 39          ; SYS_GETPID
    syscall
    
    ; rax now contains the PID.
    ; System Call 2 (PrintHex)
    mov rdi, rax
    mov rax, 2           ; SYS_PRINT_HEX
    syscall
    
    ; Print a message using SYS_WRITE (1)
    ; rdi = fd (ignored for now), rsi = buf, rdx = count
    mov rdi, 1
    lea rsi, [rel msg]
    mov rdx, 32
    mov rax, 1           ; SYS_WRITE
    syscall

.loop:
    ; Yield to other tasks
    mov rax, 24          ; SYS_YIELD
    syscall
    jmp .loop

msg: db "Ring 3: Task active and running", 10, 0

user_program_end:
