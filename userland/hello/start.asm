;
; asm entry-point for hello.elf
;

section .text

extern main
global _start:function
_start:
    ; align stack
    xchg    bx, bx
    and     esp, 0xFFFFFFF0
    call    main

    ; We don't know what to do here, so just write something to debug port
    ; and loop
    mov     esi, message

.loop:
    mov     al, [esi]
    cmp     al, 0
    je      .endloop
    out     0xE9, al
    inc     esi
    jmp     .loop

.endloop:
    jmp     .endloop

section .rodata
message: db "Userspace program ended", 10, 0

