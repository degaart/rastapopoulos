bits 32

global resume_from_interrupt
resume_from_interrupt:
    ; params
    ;   8   esp
    ;   12  eflags
    ;   16  eip
    ;   20  edi
    ;   24  esi
    ;   28  edx
    ;   32  ecx
    ;   36  ebx
    ;   40  eax
    ;   44  ebp

    ; THIS WORKS, BUT SHOULD USE IRET OR WE RISK BEING PREEMPTED AFTER LOADING FLAGS
    push ebp
    mov ebp, esp

    ; IRET stack layout: EFLAGS CS EIP
    cli
    mov esp, [ebp+8]
    push dword [ebp+12]
    push dword 0x08
    push dword [ebp+16]

    mov edi, [ebp+20]
    mov esi, [ebp+24]
    mov edx, [ebp+28]
    mov ecx, [ebp+32]
    mov ebx, [ebp+36]
    mov eax, [ebp+40]
    mov ebp, [ebp+44]

    iret
