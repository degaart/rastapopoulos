; vim: set ft=nasm:

%define MB_ALIGN (1<<0)
%define MB_MEMINFO (1<<1)
%define MB_FLAGS (MB_ALIGN|MB_MEMINFO)
%define MB_MAGIC 0x1BADB002
%define MB_CHECKSUM (-(MB_MAGIC+MB_FLAGS))

%define COM1_PORT 0x3F8

section .multiboot
    dd MB_MAGIC
    dd MB_FLAGS
    dd MB_CHECKSUM

section .text
global _start
extern kmain
_start:
    mov esp, _stacktop
    call kmain

    mov dx, 0x3f8
    mov esi, message
show_message:
    mov al, [esi]
    test al, al
    jz loop
    out dx, al
    inc esi
    jmp show_message
loop:
    cli
    hlt
    jmp loop

section .rodata
    message: db "Back to stub.asm", 10, 0

section .bss
    resb 4096
_stacktop:

    
