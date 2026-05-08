ENTRY(start)

SECTIONS {
    . = 1M;

    .boot :
    {
        KEEP(*(.multiboot_header))
    }

    .text :
    {
        *(.text .text.* .ltext)
    }

    .rodata :
    {
        *(.rodata .rodata.* .lrodata .lrodata.*)
    }

    .user_code :
    {
        *(.user_code)
    }

    .rodata :
    {
        *(.rodata)
    }

    .data :
    {
        *(.data .data.* .ldata .ldata.*)
    }

    .bss :
    {
        *(.bss .bss.* .lbss .lbss.*)
    }

    __kernel_end = .;
}
