extern main

global _startup
_startup:
    jmp short .entry
    db "Rasta Executable", 0    ; MAGIC

.entry:
    push eax
    call main

.loop:
    jmp .loop

