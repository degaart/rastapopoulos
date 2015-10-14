section .text

    global usermode_program
    usermode_program:
        xchg bx, bx
        int 0x80
        ret

section .rodata
    str: db "Hello from usermode!", 0
