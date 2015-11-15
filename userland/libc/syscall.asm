global rs_syscall
rs_syscall:
    push ebp
    mov ebp, esp
    push ebx

    mov eax, [ebp+8]
    mov ebx, [ebp+12]
    mov ecx, [ebp+16]
    mov edx, [ebp+20]
    int 0x80

    pop ebx
    pop ebp
    ret


