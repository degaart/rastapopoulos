BITS 16
SECTION .text

%define CODE32_SEL      0x08
%define DATA32_SEL      0x10
%define CODE16_SEL      0x18
%define STACK16_SEL     0x20

; ---------------------------------------------------------------------------
; void __attribute__((cdecl))
; hmemset(uint32_t vaddr, int ch, uint32_t size);
;
; Near-call cdecl stack frame after:
;
;       push bp
;       mov  bp, sp
;
;   [bp+0]   old BP
;   [bp+2]   return IP
;   [bp+4]   vaddr low word
;   [bp+6]   vaddr high word
;   [bp+8]   ch
;   [bp+10]  size
;
; Assumptions:
;   - 386+
;   - currently in real mode
;   - paging disabled
;   - A20 already enabled
;   - code resides in RAM: this routine patches its private GDT and one
;     far-jump operand
; Refer to hmemcpy.asm for more details on how the RM -> PM -> RM transition
; works
; ---------------------------------------------------------------------------
global hmemset
hmemset:
    push bp
    mov  bp, sp

    pushf
    cli

    push ds
    push es
    push bx
    push si
    push di

    ; save current GDTR
    o32  sgdt [cs:saved_gdtr]

    ; patch descriptors
    mov  ax, cs
    mov  [cs:rm_far_jump_seg], ax
    movzx eax, ax
    shl  eax, 4

    ; CODE32 base
    mov  [cs:gdt_code32 + 2], ax
    mov  edx, eax
    shr  edx, 16
    mov  [cs:gdt_code32 + 4], dl
    mov  [cs:gdt_code32 + 7], dh

    ; CODE16 base
    mov  [cs:gdt_code16 + 2], ax
    mov  [cs:gdt_code16 + 4], dl
    mov  [cs:gdt_code16 + 7], dh

    ; GDTR base
    mov  edx, eax
    add  edx, gdt_start
    mov  [cs:gdt_ptr + 2], edx

    ; stack descriptor
    ; same as current real-mode ss
    ; save original ss in bx
    mov  bx, ss
    movzx eax, bx
    shl  eax, 4
    mov  [cs:gdt_stack16 + 2], ax
    shr  eax, 16
    mov  [cs:gdt_stack16 + 4], al
    mov  [cs:gdt_stack16 + 7], ah

    ; install temporary GDT
    o32  lgdt [cs:gdt_ptr]

    ; enter PM
    mov  edx, cr0
    or   edx, 1
    mov  cr0, edx

    ; far jmp to pm32_entry
    db   0xea
    dw   pm32_entry
    dw   CODE32_SEL

bits 32
pm32_entry:
    mov  ax, STACK16_SEL
    mov  ss, ax

    mov  ax, DATA32_SEL
    mov  ds, ax
    mov  es, ax

    mov  edi, [ebp + 4]
    mov  ecx, [ebp + 10]
    mov  al, [ebp + 8]
    cld
    rep  stosb

    db   0xea
    dd   pm16_exit
    dw   CODE16_SEL

bits 16
pm16_exit:
    ; disable PM
    mov  eax, cr0
    and  eax, 0xfffffffe
    mov  cr0, eax

    ; far jump to RM
    ; the segment word was patched at function entry
rm_far_jump:
    db   0xea
    dw   rm_entry
rm_far_jump_seg:
    dw   0

rm_entry:
    mov  ss, bx
    o32 lgdt [cs:saved_gdtr]

    ; restore caller state
    pop  di
    pop  si
    pop  bx
    pop  es
    pop  ds
    popf
    pop  bp
    ret

; private GDT
align 8
gdt_start:
    ; 0x0: null descriptor
    dq   0

    ; 0x8 CODE32
gdt_code32:
    dw   0xffff
    dw   0x0000
    db   0x00
    db   10011010b
    db   11001111b
    db   0x0

    ; 0x10 DATA32
gdt_data32:
    dw   0xffff
    dw   0x0000
    db   0x00
    db   10010010b
    db   11001111b
    db   0x00

    ; 0x18 CODE16
gdt_code16:
    dw   0xffff
    dw   0x0000
    db   0x00
    db   10011010b
    db   00000000b
    db   0x00

    ; 0x20 STACK16
gdt_stack16:
    dw      0xffff
    dw      0x0000
    db      0x00
    db      10010010b
    db      00000000b
    db      0x00
gdt_end:

gdt_ptr:
    dw      gdt_end - gdt_start - 1
    dd      0

saved_gdtr:
    dw      0
    dd      0





