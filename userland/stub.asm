bits        32
org         0x400000


entry:
    mov     esi, msg

.loop:
    mov     al, [esi]
    cmp     al, 0
    je      .break

    out     0xE9, al
    inc     esi
    jmp     .loop
.break:
    jmp     $ 

msg: db "Hello, from userland!", 10, 0

