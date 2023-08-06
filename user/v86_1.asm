bits 16
org  0x100

VGA_BASE equ 0xB800

mov  ah, 0x00
mov  al, 0x12
int  0x10
mov  si, message
.loop:
mov  al, [si]
test al, al
jz   .exit
mov  ah, 0x0E
mov  bh, 0x00
mov  bl, 0x0F
int  0x10
inc  si
jmp  .loop
.exit:
jmp  $

message: db "All your base are belong to us", 13, 10, 0



