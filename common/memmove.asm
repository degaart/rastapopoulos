cpu 386

extern memcpy

section .text
global memmove:function
memmove:
    cld
    mov eax, [esp + 4]
    mov edx, [esp + 8]
    cmp eax, edx
    je .done
    jb memcpy
    mov ecx, eax
    sub ecx, edx
    cmp ecx, [esp + 12]
    jae memcpy

    push esi
    push edi
    mov ecx, [esp + 20]
    lea esi, [edx + ecx - 1]
    lea edi, [eax + ecx - 1]
    mov edx, ecx
    std
    cmp edx, 4
    jb .bytes
    cmp edx, 16
    jb .words

    lea ecx, [edi + 1]
    and ecx, 3
    sub edx, ecx
    rep movsb

.words:
    sub esi, 3
    sub edi, 3
    mov ecx, edx
    shr ecx, 2
    rep movsd
    add esi, 3
    add edi, 3
    and edx, 3
.bytes:
    mov ecx, edx
    rep movsb
    cld
    pop edi
    pop esi
.done:
    ret

