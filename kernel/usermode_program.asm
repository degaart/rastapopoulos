section .text

align 4096
global usermode_program
usermode_program:
    mov     esi, .str

    mov     dx, 0xE9                ; debug port
    xor     eax, eax
    .loop:
        mov     al, [esi]
        test    eax, eax
        je      .halt
        out     dx, al
        inc     esi
        jmp     .loop

    .halt:
        ; hlt not accessible from usermode
        jmp $

    .str:
        db "Hello, from userspace!", 10, 0


