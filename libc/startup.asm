extern main

global _startup
_startup:
    jmp short .entry
    db "Rasta Executable", 0    ; MAGIC

.entry:
    push eax

    ;mov al, '*'
    ;mov dx, 0xE9
    ;out dx, al

    call main

.loop:
    jmp .loop

