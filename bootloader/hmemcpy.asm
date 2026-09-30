BITS 16
SECTION .text

global hmemcpy


; ---------------------------------------------------------------------------
; GDT selectors
; ---------------------------------------------------------------------------

%define CODE32_SEL      0x08
%define DATA32_SEL      0x10
%define CODE16_SEL      0x18
%define STACK16_SEL     0x20


; ---------------------------------------------------------------------------
; void __attribute__((cdecl))
; hmemcpy(uint32_t dst, const void *src, size_t len);
;
; Near-call cdecl stack frame after:
;
;       push bp
;       mov  bp, sp
;
;   [bp+0]   old BP
;   [bp+2]   return IP
;   [bp+4]   dst low word
;   [bp+6]   dst high word
;   [bp+8]   src offset
;   [bp+10]  len
;
; Assumptions:
;   - 386+
;   - currently in real mode
;   - paging disabled
;   - A20 already enabled
;   - src is a gcc-ia16 near pointer relative to the normal data segment
;     (here assumed to be SS)
;   - code resides in RAM: this routine patches its private GDT and one
;     far-jump operand
; ---------------------------------------------------------------------------

hmemcpy:
    push bp
    mov  bp, sp

    ; Preserve caller flags, especially IF and DF.
    pushf
    cli

    ; Preserve segment registers which we modify.
    push ds
    push es

    ; Preserve all 32-bit GP registers that we use.
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi


    ; -----------------------------------------------------------------------
    ; Save the caller's GDTR.
    ;
    ; o32 is important on a 386+, so the complete 32-bit GDT base is saved.
    ; -----------------------------------------------------------------------

    o32 sgdt [cs:saved_gdtr]


    ; -----------------------------------------------------------------------
    ; Patch the protected-mode code descriptors.
    ;
    ; They have a base equal to the current real-mode CS << 4.
    ; This lets pm32_entry and pm16_exit continue using their ordinary
    ; 16-bit link-time offsets inside this code segment.
    ; -----------------------------------------------------------------------

    mov     ax, cs

    ; Also patch the segment part of the real-mode return far jump.
    mov     [cs:rm_far_jump_seg], ax

    movzx   eax, ax
    shl     eax, 4                  ; EAX = physical base of current CS


    ; CODE32 base

    mov     word [cs:gdt_code32 + 2], ax

    mov     edx, eax
    shr     edx, 16

    mov     byte [cs:gdt_code32 + 4], dl
    mov     byte [cs:gdt_code32 + 7], dh


    ; CODE16 base

    mov     word [cs:gdt_code16 + 2], ax
    mov     byte [cs:gdt_code16 + 4], dl
    mov     byte [cs:gdt_code16 + 7], dh


    ; -----------------------------------------------------------------------
    ; Patch GDTR base.
    ;
    ; physical address = (CS << 4) + offset(gdt_start)
    ; -----------------------------------------------------------------------

    mov     edx, eax
    add     edx, gdt_start
    mov     dword [cs:gdt_ptr + 2], edx


    ; -----------------------------------------------------------------------
    ; Construct a protected-mode stack descriptor having exactly the same
    ; physical base as the current real-mode SS.
    ;
    ; Keep the original SS in BX so we can restore it after leaving PM.
    ; -----------------------------------------------------------------------

    mov     bx, ss

    movzx   eax, bx
    shl     eax, 4                  ; EAX = physical base of current SS

    mov     word [cs:gdt_stack16 + 2], ax

    shr     eax, 16

    mov     byte [cs:gdt_stack16 + 4], al
    mov     byte [cs:gdt_stack16 + 7], ah


    ; -----------------------------------------------------------------------
    ; Prepare copy arguments while still in real mode.
    ;
    ; EDI = destination physical address
    ; ESI = source physical address
    ; ECX = byte count
    ; -----------------------------------------------------------------------

    mov     edi, dword [ss:bp + 4]

    movzx   esi, word [ss:bp + 8]
    movzx   ecx, word [ss:bp + 10]


    ; gcc-ia16 near pointer -> physical address.
    ;
    ; This assumes the normal near-data segment is SS.
    ;
    ; If your particular ABI/setup uses DS as the near-pointer base,
    ; replace "mov ax, ss" with "mov ax, ds".

    mov     ax, ss
    movzx   eax, ax
    shl     eax, 4
    add     esi, eax


    ; -----------------------------------------------------------------------
    ; Install our temporary GDT.
    ; -----------------------------------------------------------------------

    o32 lgdt [cs:gdt_ptr]


    ; -----------------------------------------------------------------------
    ; Enter protected mode.
    ;
    ; Keep the new CR0 value in EDX.  Nothing in the copy path modifies EDX,
    ; so we can clear PE again using the same value.
    ; -----------------------------------------------------------------------

    mov     edx, cr0
    or      edx, 1
    mov     cr0, edx


    ; Far JMP, 16-bit offset + 16-bit selector.
    ;
    ; pm32_entry must therefore be within the first 64 KiB of this code
    ; segment.  Loading CODE32_SEL changes the default operand/address size
    ; to 32 bits for subsequent instructions.

    db      0xEA
    dw      pm32_entry
    dw      CODE32_SEL



; ===========================================================================
; 32-bit protected mode
; ===========================================================================

BITS 32

pm32_entry:

    ; Give SS a valid protected-mode descriptor while keeping exactly
    ; the same physical stack and 16-bit SP semantics.
    ;
    ; We deliberately perform no stack operations while here.

    mov     ax, STACK16_SEL
    mov     ss, ax


    ; Flat 4-GiB data segments, base zero.

    mov     ax, DATA32_SEL
    mov     ds, ax
    mov     es, ax


    ; memcpy() semantics: forward copy.

    cld


    ; Copy dwords first.

    mov     eax, ecx
    shr     ecx, 2

    rep movsd


    ; Copy remaining 0..3 bytes.

    mov     ecx, eax
    and     ecx, 3

    rep movsb


    ; -----------------------------------------------------------------------
    ; Move to a 16-bit protected-mode code segment before clearing PE.
    ;
    ; Since the current code segment is 32-bit, this is a ptr16:32 JMP.
    ; -----------------------------------------------------------------------

    db      0xEA
    dd      pm16_exit
    dw      CODE16_SEL



; ===========================================================================
; 16-bit protected mode
; ===========================================================================

BITS 16

pm16_exit:

    ; Clear PE, leaving all other CR0 bits as they were.

    and     edx, 0xfffffffe
    mov     cr0, edx


    ; -----------------------------------------------------------------------
    ; Far jump immediately after clearing PE.
    ;
    ; The segment word was patched at function entry with the original CS.
    ; This reloads CS using real-mode semantics and flushes the instruction
    ; stream.
    ; -----------------------------------------------------------------------

rm_far_jump:
    db      0xEA
    dw      rm_entry

rm_far_jump_seg:
    dw      0



; ===========================================================================
; Back in real mode
; ===========================================================================

rm_entry:

    ; SS currently has visible value STACK16_SEL, although its cached base
    ; points at the correct physical stack.
    ;
    ; BX still contains the original real-mode SS.

    mov     ss, bx


    ; Restore the GDT which existed before this call.

    o32 lgdt [cs:saved_gdtr]


    ; Restore caller state.

    pop     edi
    pop     esi
    pop     edx
    pop     ecx
    pop     ebx
    pop     eax

    pop     es
    pop     ds

    ; Restores IF and the caller's DF.
    popf

    pop     bp
    ret



; ===========================================================================
; Private GDT
;
; This lives in the code segment because the routine needs to find and patch
; it before DS has been converted to a flat protected-mode selector.
;
; The code therefore has to be loaded into writable RAM.
; ===========================================================================

align 8

gdt_start:

    ; 0x00: mandatory null descriptor
    dq      0


; ---------------------------------------------------------------------------
; 0x08: 32-bit protected-mode code
;
; Base is patched to real-mode CS << 4.
; Limit = 4 GiB
; D = 1
; G = 1
; ---------------------------------------------------------------------------

gdt_code32:
    dw      0xffff                  ; limit 0..15
    dw      0x0000                  ; base 0..15, patched
    db      0x00                    ; base 16..23, patched
    db      10011010b               ; present, ring 0, code, readable
    db      11001111b               ; G=1, D=1, limit high=0xf
    db      0x00                    ; base 24..31, patched


; ---------------------------------------------------------------------------
; 0x10: flat 32-bit data
;
; Base = 0
; Limit = 4 GiB
; ---------------------------------------------------------------------------

gdt_data32:
    dw      0xffff
    dw      0x0000
    db      0x00
    db      10010010b               ; present, ring 0, writable data
    db      11001111b               ; G=1, D/B=1, limit high=0xf
    db      0x00


; ---------------------------------------------------------------------------
; 0x18: 16-bit protected-mode code used during the return transition
;
; Base is patched to real-mode CS << 4.
; Limit = 64 KiB - 1
; D = 0
; G = 0
; ---------------------------------------------------------------------------

gdt_code16:
    dw      0xffff
    dw      0x0000                  ; patched
    db      0x00                    ; patched
    db      10011010b
    db      00000000b               ; 16-bit, byte granularity
    db      0x00                    ; patched


; ---------------------------------------------------------------------------
; 0x20: 16-bit protected-mode stack
;
; Base is patched to the original SS << 4.
; Limit = 64 KiB - 1
; B = 0 -> SP, not ESP, is the stack pointer
; ---------------------------------------------------------------------------

gdt_stack16:
    dw      0xffff
    dw      0x0000                  ; patched
    db      0x00                    ; patched
    db      10010010b
    db      00000000b               ; 16-bit stack, byte granularity
    db      0x00                    ; patched


gdt_end:


; GDTR pseudo-descriptor.
;
; Base gets patched every call because it depends on the current CS.

gdt_ptr:
    dw      gdt_end - gdt_start - 1
    dd      0


; Original GDTR, restored before returning to C.

saved_gdtr:
    dw      0
    dd      0


