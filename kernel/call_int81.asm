section .text

global call_int81
call_int81:
    push ebp
    mov ebp, esp

    int 0x81
    xchg bx, bx

    pop ebp
    ret
