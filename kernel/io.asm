;
; IO assembly  helper functions
;

section .user

; void outb(uint16_t port, uint8_t val);
global outb
outb:
    ; stack frame
    ; uint32_t val          [ebp + 12]
    ; uint32_t port         [ebp + 8]
    ; uint32_t ret address  [ebp + 4]
    ; uint32_t ebp          [ebp]
    push    ebp
    mov     ebp, esp
    mov     dx, [ebp + 8]
    mov     al, [ebp + 12]
    out     dx, al
    pop     ebp
    ret

; uint32_t inb(uint32_t port);
global inb
inb:
    ; uint32_t port             [ebp + 8]
    ; uint32_t ret address      [ebp + 4]
    ; uint32_t ebp              [ebp]
    push    ebp
    mov     ebp, esp
    mov     dx, [ebp + 8]
    xor     eax, eax
    in      al, dx
    pop     ebp
    ret

; void outw(uint32_t port, uint32_t val);
global outw
outw:
    ; uint32_t val          [ebp + 12]
    ; uint32_t port         [ebp + 8]
    ; uint32_t ret address  [ebp + 4]
    ; uint32_t ebp          [ebp]
    push    ebp
    mov     ebp, esp
    mov     dx, [ebp + 8]
    mov     ax, [ebp + 12]
    out     dx, ax
    pop     ebp
    ret

; uint32_t inw(uint32_t port);
global inw
inw:
    ; uint32_t port         [ebp + 8]
    ; uint32_t ret address  [ebp + 4]
    ; uint32_t ebp          [ebp]
    push    ebp
    mov     ebp, esp
    mov     dx, [ebp + 8]
    xor     eax, eax
    in      ax, dx
    pop     ebp
    ret



