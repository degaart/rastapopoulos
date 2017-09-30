;
; Return to real mode and call int 0x10
; Protected mode part
;

section .text

global int10

int10:
    ; Jump to 16-bit protected mode stub
    ; 0x30: 16-bit code selector
    ; 0x7C00: load address of stub
    cli
    jmp     0x30:0x7C00
    ret


