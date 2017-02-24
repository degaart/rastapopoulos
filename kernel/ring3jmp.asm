section .text

;
; Jump into ring-3
;
global ring3jmp
ring3jmp:
    ; params
    ;   8   esp
    ;   12  eflags
    ;   16  eip
    ;   20  edi
    ;   24  esi
    ;   28  edx
    ;   32  ecx
    ;   36  ebx
    ;   40  eax
    ;   44  ebp
    push    esp
    mov     ebp, esp

    ; setup segments
    mov     ax, 0x23
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax

    ; setup iret stack layout
    push    0x23                    ; SS (USER_DATA_SEG|RPL3)
    push    dword [ebp + 8]         ; ESP
    push    dword [ebp + 12]        ; EFLAGS
    push    0x1b                    ; CS (USER_CODE_SEG|RPL3)
    push    dword [ebp + 16]        ; EIP

    mov     edi, [ebp + 20]
    mov     esi, [ebp + 24]
    mov     edx, [ebp + 28]
    mov     ecx, [ebp + 32]
    mov     ebx, [ebp + 36]
    mov     eax, [ebp + 40]
    mov     ebp, [ebp + 44]

    iret
 

