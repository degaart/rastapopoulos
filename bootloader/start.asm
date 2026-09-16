bits    16
cpu     8086

%include "util.inc"

section .text.startup
global _start
_start:
    ; setup code
    cli
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; stack at 0x7fff
    ; this is just an initial stack
    mov sp, 0x8000
    jmp 0x0:clear_bss

    ; clear bss
clear_bss:
    extern __bss_start
    extern __bss_end
    mov di, __bss_start
    mov cx, __bss_end
    sub cx, di
    jcxz call_main
    xor al, al
    rep stosb

    ; call C main function
call_main:
    extern main
    call main
    jmp halt

section .text
global halt
halt:
    cli
    hlt
    jmp $

