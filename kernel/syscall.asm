section .user

global syscall
syscall:
    push    ebp
    mov     ebp, esp

    push    ebx
    push    esi
    push    edi

    mov     eax, [ebp + 8]
    mov     ebx, [ebp + 12]
    mov     ecx, [ebp + 16]
    mov     edx, [ebp + 20]
    mov     esi, [ebp + 24]
    mov     edi, [ebp + 28]

    cmp     eax, 2
    jne     .call_int
    

.call_int:
    int     0x80

    pop     edi
    pop     esi
    pop     ebx

    pop     ebp
    ret

