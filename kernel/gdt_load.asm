section .text

; void gdt_load(const struct Gdtr* gdtr);
;
; struct Gdtr {
;     uint16_t limit;
;     uint32_t base;
; } __attribute__((packed));
;
; [ebp+8] = gdtr
global gdt_load
gdt_load:
    push ebp
    mov  ebp, esp

    mov  eax, [ebp+8]
    lgdt [eax]

    ; reload cs
    jmp 0x08:.return

.return:
    ; reload other segments
    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax

    pop  ebp
    ret


