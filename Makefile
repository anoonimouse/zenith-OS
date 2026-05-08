# TinyOS Makefile

ARCH ?= x86_64
KERNEL := kernel.bin
ISO := tinyos.iso

AS := C:\Users\uditc\AppData\Local\bin\NASM\nasm.exe
CC := C:\Users\uditc\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260505-ucrt-x86_64\bin\x86_64-w64-mingw32-gcc.exe
LD := C:\Users\uditc\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260505-ucrt-x86_64\bin\ld.lld.exe
OBJCOPY := C:\Users\uditc\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260505-ucrt-x86_64\bin\llvm-objcopy.exe
CFLAGS := -target x86_64-elf -m64 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -ffreestanding -mcmodel=large -Wall -Wextra
LDFLAGS := -m elf_x86_64 --nmagic --script=boot/linker.ld

ASM_SRC := boot/boot.asm kernel/hal/interrupts.asm kernel/proc/context.asm kernel/proc/syscall_entry.asm kernel/hal/gdt_asm.asm kernel/hal/user_asm.asm kernel/proc/user_test.asm
C_SRC := kernel/main.c kernel/hal/io.c kernel/mm/pmm.c kernel/mm/vmm.c kernel/mm/heap.c kernel/hal/idt.c kernel/hal/isr.c kernel/proc/scheduler.c kernel/hal/timer.c kernel/proc/syscall.c kernel/hal/gdt.c kernel/hal/apic.c kernel/hal/keyboard.c

ASM_OBJ := $(ASM_SRC:.asm=.o)
C_OBJ := $(C_SRC:.c=.o)

.PHONY: all clean iso run

all: $(KERNEL)

%.o: %.asm
	$(AS) -f elf64 $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(ASM_OBJ) $(C_OBJ)
	$(LD) $(LDFLAGS) -o $@ $^
	$(OBJCOPY) -I elf64-x86-64 -O elf32-i386 $@

iso: $(KERNEL)
	mkdir -p build/isofiles/boot/grub
	cp $(KERNEL) build/isofiles/boot/kernel.bin
	cp boot/grub.cfg build/isofiles/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) build/isofiles
	rm -rf build

run: iso
	qemu-system-x86_64 -cdrom $(ISO) -serial stdio

clean:
	-del /Q /S *.o 2>NUL
	-del /Q $(KERNEL) $(ISO) 2>NUL
	-rmdir /S /Q build 2>NUL
