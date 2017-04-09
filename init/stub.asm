section .text

extern main

global _entry
_entry:
    call    main

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

msg: db "Program terminated", 10, 0

