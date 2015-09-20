use16

;
; Params:
;   pointer to buffer (should be placed in conventional memory)
;   buffer size
;
; Returns:  0xFFFFFFFF on error
;           entry count on success
;
%define buffer          ebp + 8
%define buffer_size     ebp + 12
%define entry_size      24
%define magic           0x534D4150

struc entry_t
    base_lo:    resd 1
    base_hi:    resd 1
    size_lo:    resd 1
    size_hi:    resd 1
    flags:      resd 1
    xflags:     resd 1
endstruc

global _bios_memmap
_bios_memmap:
        push ebp
        mov ebp, esp
        push ebx
        push edi
        push esi
        push es

        mov edi, [buffer]       ; dest buffer
        xor ebx, ebx            ; first cookie value = 0

        mov ax, ds              ; assume buffer is in ds
        mov es, ax

        xor esi, esi            ; entry count

        ;
        ; entry format:
        ;   uint64_t    base
        ;   uint64_t    size
        ;   uint32_t    flags
        ;   uint32_t    extended flags (init to 1 before call)
        ;
    .loop:
        ; check remaining buffer size
        ; edi + entry_size < buffer + buffer_size
        mov eax, edi
        add eax, entry_size

        mov ecx, [buffer]
        add ecx, [buffer_size]
        cmp eax, ecx
        jae .err

        ; clear current buffer
        xor eax, eax
        mov [edi + base_lo], eax
        mov [edi + base_hi], eax
        mov [edi + size_lo], eax
        mov [edi + size_hi], eax
        mov [edi + flags],   eax
        mov eax,             1              
        mov [edi + xflags],  eax     ; extended flags are initialized to 0x1

        ; setup for call
        mov edx, magic
        mov eax, 0xE820
        mov ecx, 24
        int 0x15

        ; check call results
        jc  .err
        cmp eax, magic
        jne .err
        cmp ebx, 0
        je  .success

        ; if size = 0, ignore entry and dont increment buffer
        ;xchg bx, bx
        mov eax, [edi + size_lo]
        cmp eax, 0
        jne .inc_buffer
        mov eax, [edi + size_hi]
        cmp eax, 0
        jne .inc_buffer

        ; if xflags = 0, ignore entry and dont increment buffer
        mov eax, [edi + xflags]
        cmp eax, 0
        jne .inc_buffer
        jmp .loop

        ; success, increment buffer pointer
    .inc_buffer:
        add edi, entry_size
        inc esi
        jmp .loop

    .return:
        pop esi
        pop edi
        pop ebx
        pop es
        pop ebp
        retf
    .err:
        mov eax, 0xFFFFFFFF
        jmp .return
    .success:
        mov eax, esi
        jmp .return

