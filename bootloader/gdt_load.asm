bits 16
section .text

; void gdt_load(const struct Gdtr* gdtr);
;
; struct Gdtr {
;     uint16_t limit;
;     uint32_t base;
; } __attribute__((packed));
;
; [bp+4] = gdtr
global gdt_load
gdt_load:
    push bp
    mov  bp, sp
    push bx

    mov  bx, [bp + 4]

    ; Force the 32-bit operand-size form so LGDT loads
    ; the complete 32-bit base from the 6-byte GDTR.
    o32 lgdt [bx]

    pop  bx
    pop  bp
    ret


