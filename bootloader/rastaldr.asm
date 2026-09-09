bits 16
org 0x0000

; setup environment
mov ax, cs
mov ds, ax
mov es, ax
mov sp, 0x7c00

push message
call puts

halt:
    cli
    hlt
    jmp halt


; prints an asciiz string
; arg: string to print
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
    ret  2

message: db 'RASTALDR running', 13, 10, 0

