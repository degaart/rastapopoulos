section .start.text

extern __bss_end
extern __bss_start
extern main

global _start:function
_start:
    ; clear bss
    xor  eax, eax
    mov  edi, __bss_start
    mov  ecx, __bss_end
    sub  ecx, edi
    rep  stosb

    ; call main
    call main
    ret

