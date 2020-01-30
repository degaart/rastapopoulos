;
; asm entry-point for kernel
; https://wiki.osdev.org/Bare_Bones_with_NASM
;

MBALIGN  equ  1 << 0            ; align loaded modules on page boundaries
MEMINFO  equ  1 << 1            ; provide memory map
FLAGS    equ  MBALIGN | MEMINFO ; this is the Multiboot 'flag' field
MAGIC    equ  0x1BADB002        ; 'magic number' lets bootloader find the header
CHECKSUM equ -(MAGIC + FLAGS)   ; checksum of above, to prove we are multiboot

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

; stack
; marked as nobits in linker file so is not stored in kernel image
; must be 16-bytes aligned due to sysv abi
section .bss
align 16
stack_bottom:
    resb 16384                  ; 16kb
stack_top:

section .text

; entry point called by bootloader
; marked as a function
global _start:function (_start.end - _start)
_start:
    ; environment set up by bootloader:
    ;   - 32-bit protected mode
    ;   - interrupts disabled
    ;   - paging disabled
    ;   - undefined stack
    ;   - undefined gdt
    
    ; setup stack
    mov esp, stack_top
    mov [esp], ebx

    ; call kernel C entry point
    extern kmain
    call kmain

    ; if main returned, halt
    cli
.hang:
    hlt
    jmp .hang

.end:

