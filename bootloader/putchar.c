int putchar(int ch)
{
    asm volatile("cmp al, 10\n\t"
                 "jne 1f\n\t"
                 "push ax\n\t"
                 "mov al, 13\n\t"
                 "mov ah, 0xe\n\t"
                 "mov bx, 0x7\n\t"
                 "int 0x10\n\t"
                 "pop ax\n\t"
                 "1:\n\t"
                 "mov ah, 0xe\n\t"
                 "mov bx, 0x7\n\t"
                 "int 0x10"
                 : "+a"(ch)
                 :
                 : "bx", "bp", "cc", "memory");
    return ch;
}

