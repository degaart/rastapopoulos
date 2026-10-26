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

section .note.GNU-stack noalloc noexec nowrite progbits
