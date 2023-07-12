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
    add  esp, 8

    ; void trace(const char* file, int line, const char* fn, const char* fmt, ...)
    push halted_message
    push start_fn
    push dword __LINE__
    push current_file
    extern trace
    call trace
    add  esp, 16
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
    halted_message: db "SYSTEM HALTED", 10, 0
    start_fn: db "_start", 0
    current_file: db "stub.asm", 0

section .bss
    resb 4096
stacktop:

