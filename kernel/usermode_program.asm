;
; Unit-test for ring3 jumps
; 1. Print test string to debug port
; 2. xor all registers with 7 so kernel can compare it back later
; 3. call int 0x80 to return to kernel mode
;
section .text

align 4096
global usermode_program
usermode_program:
    ; save regs
    pusha

    ; Show debug string
    mov     esi, .str
    call    trace

    ; xor regs with 7
    popa
    xor     edi, 7
    xor     esi, 7
    xor     edx, 7
    xor     ecx, 7
    xor     ebx, 7
    xor     eax, 7
    xor     ebp, 7

    ; Return to ring0
    int     0x80

    .str:
        db      "Hello, from userspace!", 10, 0

trace:
    xor     eax, eax
    mov     dx, 0xE9

    .loop:
        mov     al, [esi]
        test    al, al
        jz      .return

        out     dx, al
        inc     esi
        jmp     .loop

    .return:
        ret


