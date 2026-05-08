section .text
bits 64

global gdt_load
global tss_load

gdt_load:
    lgdt [rdi]
    ; Reload code segment
    push 0x08
    lea rax, [rel .reload_segments]
    push rax
    retfq

.reload_segments:
    ; Reload data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

tss_load:
    mov ax, 0x28 ; TSS selector (0x28 = 5 * 8)
    ltr ax
    ret
