cpu 386

%define MODE_X_STRIDE 80

section .text
global vga_pack_plane:function
vga_pack_plane:
    push esi
    push edi
    mov edi, [esp + 12]
    mov esi, [esp + 16]
    mov ecx, [esp + 20]
    mov edx, ecx
    shr ecx, 2
    jz .tail
.pack:
    mov al, [esi]
    mov ah, [esi + 4]
    shl eax, 16
    mov al, [esi + 8]
    mov ah, [esi + 12]
    ror eax, 16
    mov [edi], eax
    add esi, 16
    add edi, 4
    dec ecx
    jnz .pack
.tail:
    and edx, 3
    jz .done
.byte:
    mov al, [esi]
    mov [edi], al
    add esi, 4
    inc edi
    dec edx
    jnz .byte
.done:
    pop edi
    pop esi
    ret

global vga_pack_rows:function
vga_pack_rows:
    push ebx
    push esi
    push edi
    push ebp
    mov edi, [esp + 20]
    mov esi, [esp + 24]
    mov ebx, [esp + 28]
    mov ebp, [esp + 32]
.row:
    mov ecx, ebx
    shr ecx, 2
    jz .tail
.pack:
    mov al, [esi]
    mov ah, [esi + 4]
    shl eax, 16
    mov al, [esi + 8]
    mov ah, [esi + 12]
    ror eax, 16
    mov [edi], eax
    add esi, 16
    add edi, 4
    dec ecx
    jnz .pack
.tail:
    mov ecx, ebx
    and ecx, 3
    jz .next
.byte:
    mov al, [esi]
    mov [edi], al
    add esi, 4
    inc edi
    dec ecx
    jnz .byte
.next:
    mov edx, ebx
    shl edx, 2
    sub esi, edx
    add esi, 320
    dec ebp
    jnz .row
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret

global vga_copy_rows:function
vga_copy_rows:
    push ebx
    push esi
    push edi
    push ebp
    mov edi, [esp + 20]
    mov esi, [esp + 24]
    mov ebx, [esp + 28]
    mov edx, [esp + 32]
    mov eax, 80
    sub eax, ebx
    mov ebp, ebx
    shr ebp, 2
    and ebx, 3
    cld
.row:
    mov ecx, ebp
    rep movsd
    mov ecx, ebx
    rep movsb
    add edi, eax
    dec edx
    jnz .row
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret

