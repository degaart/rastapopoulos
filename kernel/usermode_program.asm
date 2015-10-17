section .text

    global usermode_program
    usermode_program:
        ;xchg bx, bx
        mov eax, 1          ; syscall: write
        mov ecx, str        ; arg0: string to write
        int 0x80

        mov eax, 0          ; syscall: halt
        int 0x80
        ret

section .rodata
    str: db "Hello from usermode!", 0
