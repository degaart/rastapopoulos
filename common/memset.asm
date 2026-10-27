cpu 386

section .text
global memset:function
memset:
    push edi
    mov edi, [esp + 8]
    mov al, [esp + 12]
    mov edx, [esp + 16]
    cld
    cmp edx, 4
    jb .bytes

    mov ah, al
    mov ecx, eax
    shl eax, 16
    mov ax, cx
    cmp edx, 16
    jb .words

    mov ecx, edi
    neg ecx
    and ecx, 3
    sub edx, ecx
    rep stosb

.words:
    mov ecx, edx
    shr ecx, 2
    rep stosd
    and edx, 3
.bytes:
    mov ecx, edx
    rep stosb
    mov eax, [esp + 8]
    pop edi
    ret

