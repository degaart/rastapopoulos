; Constants
MBALIGN     equ 1 << 0                      ; page-align modules
MEMINFO     equ 1 << 1                      ; provide memmap
MBFLAGS     equ MBALIGN | MEMINFO
MAGIC       equ 0x1BADB002
CHECKSUM    equ -(MAGIC + MBFLAGS)

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
    ; eax = 0x2BADB002
    ; ebx = multiboot info

    ; set up stack
    mov esp, stack_top

    ; call C entry point
    push ebx
    push eax
    extern kmain
    call kmain
    add esp, 8

    ; infinite loop
.hlt:
    cli
    hlt
    jmp .hlt

.end:

