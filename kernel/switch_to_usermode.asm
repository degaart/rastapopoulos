section .text

extern set_kernel_stack

global switch_to_usermode
switch_to_usermode:
    ;xchg bx, bx
    ;cli

    ;push esp
    ;call set_kernel_stack
    ;pop eax

    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov eax, esp
    push 0x23       ; SS (USER_DATA_SEG|RPL3)
    push eax        ; ESP
    pushf           ; EFLAGS
    push 0x1b       ; CS (USER_CODE_SEG|RPL3)
    push .ret       ; EIP
    iret

.ret:
    ret
