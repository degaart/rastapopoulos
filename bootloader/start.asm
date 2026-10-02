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
    jmp 0x0:check_386

check_386:
    ; detect 386 by changing flags bits 12..15
    pushf
    pop dx
    mov ax, dx
    or ax, 0xf000
    push ax
    popf
    pushf
    pop ax
    push dx
    popf
    and ax, 0xf000
    jnz clear_bss

    mov si, not_386
.message_loop:
    mov al, [si]
    test al, al
    jz halt
    mov ah, 0xe
    mov bx, 0x7
    int 0x10
    inc si
    jmp .message_loop

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

;section .rodata
not_386: db `This operating system requires a 386+\r\n`, 0

