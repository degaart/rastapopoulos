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
    ; multiboot state:
    ; eax   magic value 0x2BADB002
    ; ebx   physical address of multiboot information structure
    ; cr0   PE enabled, PG disabled
    ; gdtr  undefined, so must set GDT
    ; idtr  undefined, so must set IDT
    cli
    mov esp, stacktop
    push eax
    push ebx
    call kmain

    mov dx, 0x3F8
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

; void gdt_flush(void* gdtr)
global gdt_flush
gdt_flush:
    mov     eax, [esp+4]        ; should be pointer to gdt_ptr in gdt.cpp
    lgdt    [eax]

    mov     ax, 0x10            ; kernel data segment descriptor
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax
    jmp     0x08:.return        ; 0x08: kernel code segment descriptor
.return:
    ret

section .rodata
    message: db "Kernel terminated", 10, 0

section .bss
    resb 4096
stacktop:

