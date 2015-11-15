extern main

extern rs_syscall

global _startup
_startup:
    jmp short .entry
    db "Rasta Executable", 0    ; MAGIC

.entry:
    push eax
    call main

    push 0
    push 0
    push 0
    push 0x3            ; SYSCALL_EXIT
    call rs_syscall

.loop:
    jmp .loop
