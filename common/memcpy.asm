cpu 386

section .text
global memcpy:function
memcpy:
    push esi
    push edi
    mov eax, [esp + 12]
    mov esi, [esp + 16]
    mov edx, [esp + 20]
    mov edi, eax
    cld
    cmp edx, 4
    jb .bytes
    cmp edx, 16
    jb .words

    mov ecx, edi
    neg ecx
    and ecx, 3
    sub edx, ecx
    rep movsb

.words:
    mov ecx, edx
    shr ecx, 2
    rep movsd
    and edx, 3
.bytes:
    mov ecx, edx
    rep movsb

    pop edi
    pop esi
    ret

