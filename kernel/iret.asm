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

global iret
iret:
    ;
    ; params: iret_t   ESP+4
    ;

    ; This function is not reentrant
    ;xchg bx, bx
    cli

    ; save parameters in static space as we might switch esp
    mov     edi, iret_data
    mov     esi, [esp + 4]
    mov     ecx, iret_t_size
    rep     movsb

    ; switch pagedir
    ; mov     eax, [iret_data + iret_t.i_cr3]
    ; mov     cr3, eax
    
    ; Segment regs
    mov     eax, [iret_data + iret_t.i_ds]
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax

    ; Switch stack
    mov     esp, [iret_data + iret_t.i_esp]

    ; iret stack layout
    mov     eax, [iret_data + iret_t.i_cs]
    and     eax, 0x03
    cmp     eax, 0x03
    jne     .no_stack_switch

    ; if we are switching into ring3, we need to push esp and ss
    push    dword [iret_data + iret_t.i_ss]
    push    dword [iret_data + iret_t.i_esp]

.no_stack_switch:
    ; push the rest of iret regs
    push    dword [iret_data + iret_t.i_eflags]
    push    dword [iret_data + iret_t.i_cs]
    push    dword [iret_data + iret_t.i_eip]

    ; set general-purpose registers
    mov     edi, [iret_data + iret_t.i_edi]
    mov     esi, [iret_data + iret_t.i_esi]
    mov     edx, [iret_data + iret_t.i_edx]
    mov     ecx, [iret_data + iret_t.i_ecx]
    mov     ebx, [iret_data + iret_t.i_ebx]
    mov     eax, [iret_data + iret_t.i_eax]
    mov     ebp, [iret_data + iret_t.i_ebp]
    
    ; perform iret
    iret

section .bss
    iret_data: resb iret_t_size


