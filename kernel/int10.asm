;
; Return to real mode and call int 0x10
; Protected mode part
;

section .text

%include "int10.inc"

;
; param0        [ebp + 8] 0x7D00      [INT10_SCRATCH + 0]    uint32_t[8]. Registers to pass to int 10h.
; esp                     0x7D20      [INT10_SCRATCH + 32]   Original esp value.
; .return_addr            0x7D24      [INT10_SCRATCH + 36]   Protected mode return address.
; cs                      0x7D28      [INT10_SCRATCH + 40]   PM code segment. Zero-extended.
; ds                      0x7D2C      [INT10_SCRATCH + 44]   PM data segment. Zero-extended.
; idtr                    0x7D30      [INT10_SCRATCH + 48]   Original IDTR. 8 bytes.
; gdtr                    0x7D38      [INT10_SCRATCH + 56]   Original GDTR. 12 bytes.
; sentinel                0x7D44      [INT10_SCRATCH + 68]   Sentinel. 0xDEADBEEF
;
global int10
int10:
    push    ebp
    mov     ebp, esp

    pusha                               ; preserve regs

    ; Save function parameters into a known place
%macro store 1
    mov     eax, [esi + %1]
    mov     [INT10_SCRATCH + %1], eax
%endmacro

    mov     esi, [ebp + 8]
    store   P_EAX
    store   P_EBX
    store   P_ECX
    store   P_EDX
    store   P_EBP
    store   P_ESI
    store   P_EDI
    pushf
    pop     eax
    mov     [INT10_SCRATCH + P_EFLAGS], eax

    mov     eax, esp                    ; save esp
    mov     [INT10_SCRATCH + P_ESP], eax

    mov     eax, return_addr            ; save PM return addr
    mov     [INT10_SCRATCH + P_RET], eax

    xor     eax, eax                    ; save cs
    mov     ax, cs
    mov     [INT10_SCRATCH + P_CS], eax

    mov     ax, ds                      ; save ds
    mov     [INT10_SCRATCH + P_DS], eax

    xor     eax, eax
    mov     [INT10_SCRATCH + P_IDTR], eax
    mov     [INT10_SCRATCH + P_IDTR + 4], eax

    sidt    [INT10_SCRATCH + P_IDTR]    ; save IDTR
    mov     ax, [INT10_SCRATCH + P_IDTR]
    mov     ebx, [INT10_SCRATCH + P_IDTR + 2]

    xor     eax, eax
    mov     [INT10_SCRATCH + P_GDTR], eax
    mov     [INT10_SCRATCH + P_GDTR + 4], eax
    sgdt    [INT10_SCRATCH + P_GDTR]    ; save GDTR
    mov     eax, [INT10_SCRATCH + P_GDTR]
    mov     ebx, [INT10_SCRATCH + P_GDTR + 4]

    mov     eax, INT10_SENTINEL         ; sentinel
    mov     [INT10_SCRATCH + P_SENTINEL], eax

    ; Jump to 16-bit protected mode stub
    ; 0x30: 16-bit code selector
    ; 0x7C00: load address of stub
    cli
    jmp     0x30:INT10_ORG
    ret

return_addr:
    ; BUG after jumping here. The C code bombs out
    ; Seems there is a stack corruption, but esp is valid

    ; 16-bit code should return here
    break
    popa
    pop     ebp
    ret


