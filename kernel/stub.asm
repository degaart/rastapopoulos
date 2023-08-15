; vim: set ft=nasm:

%include "isr_regs.inc"

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
    mov esp, _stacktop
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

; void v86_enter(struct isr_regs* regs)
global v86_enter
v86_enter:
    push ebp
    mov  ebp, esp

    push ebx
    push esi
    push edi

    pushf
    cli

    extern tss_set_esp0
    push esp
    call tss_set_esp0
    add  esp, 4

    mov  eax, [ebp+8]
    push DWORD [eax+isr_regs.v86_gs]
    push DWORD [eax+isr_regs.v86_fs]
    push DWORD [eax+isr_regs.v86_ds]
    push DWORD [eax+isr_regs.v86_es]
    push DWORD [eax+isr_regs.ss]
    push DWORD [eax+isr_regs.esp]
    push DWORD [eax+isr_regs.eflags]
    push DWORD [eax+isr_regs.cs]
    push DWORD [eax+isr_regs.eip]
    mov  edi,  [eax+isr_regs.edi]
    mov  esi,  [eax+isr_regs.esi]
    mov  ebp,  [eax+isr_regs.ebp]
    mov  ebx,  [eax+isr_regs.ebx]
    mov  edx,  [eax+isr_regs.edx]
    mov  ecx,  [eax+isr_regs.ecx]
    mov  eax,  [eax+isr_regs.eax]
    iret

; void v86_return(struct isr_regs*)
global v86_return
v86_return:
    cli

    mov  esi, [esp+4]                       ; regs from int

    ; restore esp
    extern tss_get_esp0
    call tss_get_esp0
    mov  esp, eax

    mov  edi, [esp+24]                      ; regs from v86_enter
    mov  ecx, isr_regs_size
    rep  movsb

    popf
    pop  edi
    pop  esi
    pop  ebx
    pop  ebp
    ret

section .rodata
    halted_message: db "SYSTEM HALTED", 10, 0
    start_fn: db "_start", 0
    current_file: db "stub.asm", 0

section .bss
align 4096
    resb 8192
global _stacktop
_stacktop:


