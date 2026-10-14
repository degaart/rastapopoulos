section .text

extern kmain

global higher_half_start:function
higher_half_start:
    cli
    mov  esp, initial_stack_top
    and  esp, -16                   ; align to 16 bytes

    push ecx
    push ebx
    push eax
    call kmain
    add  esp, 3*4
.halt:
    cli
    jmp  .halt

section .bss
global initial_stack
global initial_stack_top
initial_stack: resb 16384
initial_stack_top:


