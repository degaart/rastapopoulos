section .text

extern runtime_entry

global _entry
_entry:
    call    runtime_entry
    mov     eax, 0          ; SYSCALL_EXIT
    int     0x80

