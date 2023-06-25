bits 16
org  0x7e00

    mov  si, message
    mov  ah, 0x0E

write_char:
    mov  al, [si]
    test al, al
    je   halt
    mov  bh, 0
    mov  bl, 0x07
    int  0x10
    inc  si
    jmp  write_char

halt:
    cli
    hlt
    jmp halt

message: db "It works!", 13, 10, 0

    
