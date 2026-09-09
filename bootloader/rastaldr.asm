; By default, GCC outputs code for this calling convention:
;
;   A function’s caller pushes the function’s arguments onto the stack, and pops off the stack space used to pass arguments.
;   ax, bx, cx, and dx are call-used.
;   si, di, bp, and es are call-saved.
;   ds is a special case. Both ds and ss are assumed to point to the program’s data segment on entry, and ds should be restored on exit. A function is free to modify ds, but it should restore it to its initial value—perhaps via a ‘pushw %ss; popw %ds’—before calling another function, and before returning to its caller.
bits 16

section .text.startup
global _start
_start:
    ; setup environment
    cli
    mov  ax, cs
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    mov  sp, 0xffff

    ; Detect 386+ by checking flags bit 12-16 can be changed
    pushf
    pop  bx                     ; original flags

    mov  ax, bx
    xor  ax, 0xf000             ; invert bit 12-15
    push ax
    popf

    pushf
    pop  ax
    xor  ax, bx                 ; check which bits have changed
    and  ax, 0xf000
    jz   .unsupported

    push bx
    popf                        ; restore original flags (is this necessary?)

    ; clear bss
    extern __bss_start
    extern __bss_size
    xor  ax, ax
    mov  di, __bss_start
    mov  cx, __bss_size
    rep  stosb

    ; call C entry
    extern ldrmain
    call ldrmain
    jmp  halt

.unsupported:
    push unsupported_cpu
    call puts

    jmp  halt
.end:

section .text
global halt
halt:
    cli
    hlt
    jmp halt

; prints a character
; arg: int: char to print
global putc
putc:
    push si
    mov  si, sp
    mov  si, [si+4]
    mov  ah, 0x0E
    mov  bx, 0x0007
    int  0x10
    pop  si
    ret

; prints an asciiz string
; arg: string to print
global puts
puts:
    push bp
    mov  bp, sp

    push si
    push bx

    mov  si, [bp+4]
    mov  ah, 0x0E
    mov  bx, 0x0007

.loop:
    mov  al, [si]
    test al, al
    jz   .return
    int  0x10
    inc  si
    jmp  .loop
.return:
    pop  bx
    pop  si
    pop  bp
    ret

section .rodata
message: db 'RASTALDR running', 13, 10, 0
message2: db 'RASTALDR still running', 13, 10, 0
unsupported_cpu: db 'Unsupported CPU. A 386+ is required.', 13, 10, 0

