section .text

struc iret_t
    .i_cs      resd 1
    .i_ds      resd 1
    .i_ss      resd 1
    .i_cr3     resd 1
    .i_esp     resd 1
    .i_eflags  resd 1
    .i_eip     resd 1
    .i_edi     resd 1
    .i_esi     resd 1
    .i_edx     resd 1
    .i_ecx     resd 1
    .i_ebx     resd 1
    .i_eax     resd 1
    .i_ebp     resd 1
endstruc

global perform_iret
perform_iret:
    ;
    ; params: iret_t   ESP+4
    ;

    ; This function is not reentrant
    xchg bx, bx
    cli

    ; save parameters in static space as we are going to switch pagedir
    mov edi, iret_data
    mov esi, [esp+4]
    mov ecx, iret_t_size
    rep movsb

    ; now we can switch pagedir
    mov eax, [iret_data + iret_t.i_cr3]
    mov cr3, eax

    ; switch to process's stack already (as we can't trust our own stack anymore)
    mov esp, [iret_data + iret_t.i_esp]

    ; segment regs
    mov eax, [iret_data + iret_t.i_ds]
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; setup iret stack layout
    push dword [iret_data + iret_t.i_ss]        ; SS (USER_DATA_SEG|RPL3)
    push dword [iret_data + iret_t.i_esp]       ; ESP
    push dword [iret_data + iret_t.i_eflags]    ; EFLAGS
    push dword [iret_data + iret_t.i_cs]        ; CS (USER_CODE_SEG|RPL3)
    push dword [iret_data + iret_t.i_eip]       ; EIP

    ; store regs
    mov edi, [iret_data + iret_t.i_edi]
    mov esi, [iret_data + iret_t.i_esi]
    mov edx, [iret_data + iret_t.i_edx]
    mov ecx, [iret_data + iret_t.i_ecx]
    mov ebx, [iret_data + iret_t.i_ebx]
    mov eax, [iret_data + iret_t.i_eax]
    mov ebp, [iret_data + iret_t.i_ebp]

    xchg bx, bx
    iret


section .bss
    iret_data: resb iret_t_size    
