bits 32
org 0x400000

entry:
    mov al, 'X'
    mov dx, 0xE9
    out dx, al

    mov eax, 1
    mov ebx, str
    xor ecx, ecx
    xor edx, edx
    int 0x80

.loop:
    jmp short .loop

str: db "CAN HAZ CHEEBURGER?", 0






