extern main

global _startup
_startup:
    call main

.loop:
    jmp .loop

