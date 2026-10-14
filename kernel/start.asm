extern bootstrap

; start function
section .bootstrap.text
global _start:function (_start.end - _start)
_start:
    ; 32-bit protected mode, interrupts disabled, paging disabled
    ; eax = 0x2BADB002
    ; ebx = multiboot info

    ; set up stack
    cli
    mov  esp, bootstrap_stack_top
    and  esp, -16                        ; align to 16 bytes

    ; call C bootstrapper
    push ebx
    push eax
    call bootstrap
    add  esp, 2*4
    jmp  bootstrap_halt
.end:

global bootstrap_halt:function
bootstrap_halt:
    cli
    jmp  bootstrap_halt

section .bootstrap.bss nobits alloc write align=16
; bootstrap stack
bootstrap_stack: resb 16384
bootstrap_stack_top:

