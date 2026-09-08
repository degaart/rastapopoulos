bits 16
org 0x0000

mov si, message
call puts
jmp $


; in:       si = asciiz string to print
; clobbers: ax, bx, si
puts:
    mov ah, 0x0E
    mov bx, 0x0007

.loop:
    mov al, [si]
    test al, al
    jz .return
    int 0x10
    inc si
    jmp .loop
.return:
    ret

message: db 'RASTALDR running', 13, 10, 0

