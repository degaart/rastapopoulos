extern main

global _startup
_startup:
    push eax
    call main

.loop:
    jmp .loop

