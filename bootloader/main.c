#include <stddef.h>

void putc(int ch)
{
    asm(
        "    mov al, '*'\n"
        "    mov ah, 0xe\n"
        "    mov bx, 0x7\n"
        "    int 0x10\n"
   );
}

void main()
{
    putc('%');
}

