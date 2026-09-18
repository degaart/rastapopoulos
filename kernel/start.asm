; Constants
MBALIGN     equ 1 << 0                      ; page-align modules
MEMINFO     equ 1 << 1                      ; provide memmap
MBFLAGS     equ MBALIGN | MEMINFO
MAGIC       equ 0x1BADB002
CHECKSUM    equ -(MAGIC + MBFLAGS)

; VGA text-mode cell contents
%define VGA_CHARACTER   '*'
%define VGA_FOREGROUND  0x0f
%define VGA_BACKGROUND  0x00

; Multiboot header
section .multiboot
align 4
    dd MAGIC
    dd MBFLAGS
    dd CHECKSUM

; boot stack
section .bss
align 16
stack_bottom:
resb 16384
stack_top:

; start function
section .text
global _start:function (_start.end - _start)
_start:
    ; 32-bit protected mode, interrupts disabled, paging disabled
    ; set up stack
    mov esp, stack_top

    ; Write a white '*' on black at the top-left VGA text cell.
    mov word [0xb8000], (VGA_BACKGROUND << 12) | (VGA_FOREGROUND << 8) | VGA_CHARACTER

    extern kmain
    call kmain

    ; infinite loop
.hlt:
    cli
    hlt
    jmp .hlt

.end:

