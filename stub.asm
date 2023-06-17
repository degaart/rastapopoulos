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
_start:
    mov dx, 0x3f8
    mov al, '*'
    out dx, al
    mov al, '-'
    out dx, al
    mov al, '/'
    out dx, al
loop:
    cli
    hlt
    jmp loop

    
