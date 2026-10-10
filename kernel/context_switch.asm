section .text

%define old_esp ebp+8
%define new_esp ebp+12

; void context_switch(uint32_t* old_esp, uint32_t new_esp);
global context_switch
context_switch:
    push ebp
    mov  ebp, esp

    push ebx
    push esi
    push edi
    pushf
    cli

    mov  ebx, [old_esp]
    mov  [ebx], esp

    mov  esp, [new_esp]

    popf
    pop  edi
    pop  esi
    pop  ebx
    pop  ebp
    ret

