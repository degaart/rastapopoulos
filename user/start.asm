section .text

extern __bss_end
extern __bss_start
extern main

global _start:function
_start:
    ; clear bss
    mov  ecx, __bss_end
    sub  ecx, __bss_start
    mov  edi, __bss_start
.loop:
    test ecx, ecx
    jz   .call_main
    mov  [edi], 0
    inc  edi
    dec  ecx
    jmp  .loop
.call_main:
    call main
    ret

