section .text

extern main

global _entry
_entry:
    call    main
    mov     eax, 0          ; SYSCALL_EXIT
    int     0x80

