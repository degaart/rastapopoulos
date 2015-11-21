section .text

extern set_kernel_stack

%macro save_param 2
    mov eax, %2
    mov [%1], eax
%endmacro


global switch_to_usermode
switch_to_usermode:
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
    ;   48  cr3
    push esp
    mov ebp, esp

    ; This function is not reentrant (uses static storage)
    cli

    ; save all params in static space as we can't trust the stack anymore after switching pagedir
    save_param proc_esp, [ebp+8]
    save_param proc_eflags, [ebp+12]
    save_param proc_eip, [ebp+16]
    save_param proc_edi, [ebp+20]
    save_param proc_esi, [ebp+24]
    save_param proc_edx, [ebp+28]
    save_param proc_ecx, [ebp+32]
    save_param proc_ebx, [ebp+36]
    save_param proc_eax, [ebp+40]
    save_param proc_ebp, [ebp+44]
    save_param proc_cr3, [ebp+48]

    ; we can now switch pagedir
    xchg bx, bx
    mov eax, [proc_cr3]
    mov cr3, eax
    
    ; setup segments
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; setup iret stack layout
    push 0x23                   ; SS (USER_DATA_SEG|RPL3)
    push dword [proc_esp]       ; ESP
    push dword [proc_eflags]    ; EFLAGS
    push 0x1b                   ; CS (USER_CODE_SEG|RPL3)
    push dword [proc_eip]       ; EIP

    mov edi, [proc_edi]
    mov esi, [proc_esi]
    mov edx, [proc_edx]
    mov ecx, [proc_ecx]
    mov ebx, [proc_ebx]
    mov eax, [proc_eax]
    mov ebp, [proc_ebp]

    iret

section .bss
    proc_esp:       resd 1
    proc_eflags:    resd 1
    proc_eip:       resd 1
    proc_edi:       resd 1
    proc_esi:       resd 1
    proc_edx:       resd 1
    proc_ecx:       resd 1
    proc_ebx:       resd 1
    proc_eax:       resd 1
    proc_ebp:       resd 1
    proc_cr3:       resd 1
