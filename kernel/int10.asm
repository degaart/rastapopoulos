;
; Return to real mode and call int 0x10
; Protected mode part
;

section .text

%define STATE_BASE      0x7D00


global int10

;
; param0        [ebp + 8] 0x7D00      [STATE_BASE + 0]    uint32_t[8]. Registers to pass to int 10h.
; esp                     0x7D20      [STATE_BASE + 32]   Original esp value.
; .return_addr            0x7D24      [STATE_BASE + 36]   Protected mode return address.
; cs                      0x7D28      [STATE_BASE + 40]   PM code segment. Zero-extended.
; ds                      0x7D2C      [STATE_BASE + 44]   PM data segment. Zero-extended.
; idtr                    0x7D30      [STATE_BASE + 48]   Original IDTR. 8 bytes.
; gdtr                    0x7D38      [STATE_BASE + 56]   Original GDTR. 12 bytes.
; sentinel                0x7D44      [STATE_BASE + 68]   Sentinel. 0xDEADBEEF
;
int10:
    push    ebp
    mov     ebp, esp

    pusha                               ; preserve regs


    ; Save function parameters into a known place
    xchg    bx, bx
    mov     ecx, 8                      ; 8 bytes
    lea     esi, [ebp + 8]              ; start at ebp + 8
    mov     esi, [esi]
    mov     edi, STATE_BASE             ; store into STATE_BASE

.save_params:
    mov     eax, [esi]
    mov     [edi], eax
    add     edi, 4
    add     esi, 4
    dec     ecx
    jecxz   .save_rest
    jmp     .save_params

.save_rest:
    mov     eax, esp                    ; save esp
    mov     [edi], eax
    add     edi, 4

    mov     eax, .return_addr           ; save PM return addr
    mov     [edi], eax
    add     edi, 4

    xor     eax, eax                    ; save cs
    mov     ax, cs
    mov     [edi], eax
    add     edi, 4

    mov     ax, ds                      ; save ds
    mov     [edi], eax
    add     edi, 4

    sidt    [edi]                       ; save IDTR
    add     edi, 8

    sgdt    [edi]                       ; save GDTR
    add     edi, 12

    mov     eax, 0xDEADBEEF             ; sentinel
    mov     [edi], eax

.jmp16:
    ; Jump to 16-bit protected mode stub
    ; 0x30: 16-bit code selector
    ; 0x7C00: load address of stub
    cli
    jmp     0x30:0x7C00
    ret

.return_addr:
    ; 16-bit code should return here
    popa
    pop     ebp
    ret


