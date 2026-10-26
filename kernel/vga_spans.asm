bits 32
cpu 386

%define MODE_X_STRIDE 80

section .text
global vga_fill_rows:function

vga_fill_rows:
    push ebx
    push esi
    push edi
    push ebp
    mov edi, [esp + 20]
    mov ecx, [esp + 24]
    mov ebx, [esp + 28]
    mov al, [esp + 32]
    cld
    cmp ecx, 1
    je .single
    mov ah, al
    cmp ecx, 2
    je .pair
    cmp ecx, 3
    je .triple

    mov edx, eax
    shl eax, 16
    mov ax, dx
    mov edx, MODE_X_STRIDE
    sub edx, ecx
    xor ebp, ebp
    cmp ecx, 16
    jb .prepare
    mov ebp, edi
    neg ebp
    and ebp, 3
.prepare:
    sub ecx, ebp
    mov esi, ecx
    shr esi, 2
    and ecx, 3
    test ebp, ebp
    jz .aligned_prepare
    shl ecx, 2
    or ebp, ecx
.rows:
    mov ecx, ebp
    and ecx, 3
    rep stosb
    mov ecx, esi
    rep stosd
    mov ecx, ebp
    shr ecx, 2
    rep stosb
    add edi, edx
    dec ebx
    jnz .rows
    jmp .done

.aligned_prepare:
    mov ebp, ecx
.aligned_rows:
    mov ecx, esi
    rep stosd
    mov ecx, ebp
    rep stosb
    add edi, edx
    dec ebx
    jnz .aligned_rows
    jmp .done

.single:
    mov [edi], al
    add edi, MODE_X_STRIDE
    dec ebx
    jnz .single
    jmp .done
.pair:
    mov [edi], ax
    add edi, MODE_X_STRIDE
    dec ebx
    jnz .pair
    jmp .done
.triple:
    mov [edi], ax
    mov [edi + 2], al
    add edi, MODE_X_STRIDE
    dec ebx
    jnz .triple
.done:
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret

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

section .note.GNU-stack noalloc noexec nowrite progbits
