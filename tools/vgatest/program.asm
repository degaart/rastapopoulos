BITS 16
ORG 0x7C00

start:
    mov ax, 0xDEAD
    mov eax, 0xDEADBEEF
    jmp halt

halt:
    jmp halt


