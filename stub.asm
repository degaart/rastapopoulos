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
    mov esp, _stacktop
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

; void test_idt()
extern serial_write_string
global test_idt
test_idt:
    push esp
    push idt_message1
    call serial_write_string
    add esp, 4

    mov eax, int80_handler
    mov word [idt + (8*80)], ax
    mov word [idt + (8*80) + 2], 0x08
    mov word [idt + (8*80) + 4], 0x8E00
    shr eax, 16
    mov word [idt + (8*80) + 6], ax

    lidt [idtr]

    int 80

    pop esp
    ret

int80_handler:
    pusha

    ; save segment registers
    push ds
    push es
    push fs
    push gs

    ; set data segments to kernel data segment descriptor
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    ; and what about ss?

    push idt_message2
    call serial_write_string
    add esp, 4

    ; restore data segments
    pop gs
    pop fs
    pop es
    pop ds

    popa
    iret

section .rodata
    message: db "Kernel terminated", 10, 0
    idt_message1: db "Testing IDT", 10, 0
    idt_message2: db "Inside int 3 handler", 10, 0

    align 8
    idt:
        times 255 dq 0

    idtr:
        .limit      dw (8 * 255) - 1
        .base       dd idt

section .bss
    resb 4096
_stacktop:

