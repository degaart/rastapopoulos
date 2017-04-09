section .text

global _entry
_entry:
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

section .rodata

msg: db "Hello, from userland!", 10, 0

