bits 16
section .text

entry:
    xchg bx, bx
    cli
    cld
    xor  ax, ax
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    mov  sp, 0x7DFF
    jmp  0x0:entry2

entry2:
    extern start
    call start

halt:
    cli
    hlt
    jmp  halt

