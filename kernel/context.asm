section .text

%include "context.inc"

global switch_context
switch_context:
    ;
    ; params: context_t   ESP+4
    ;

    ; This function is not reentrant
    ;xchg bx, bx
    cli

    ; save parameters in static space as we might switch esp
    mov     edi, iret_data
    mov     esi, [esp + 4]
    mov     ecx, context_t_size
    rep     movsb

    ; switch pagedir
    mov     eax, [iret_data + context_t.i_cr3]
    mov     cr3, eax
    
    ; Segment regs
    mov     eax, [iret_data + context_t.i_ds]
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax


    ; iret stack layout
    mov     eax, [iret_data + context_t.i_cs]
    and     eax, 0x03
    cmp     eax, 0x03
    jne     .switch_stack

    ; if we are switching into ring3, we need to push esp and ss
    push    dword [iret_data + context_t.i_ss]
    push    dword [iret_data + context_t.i_esp]
    jmp     .continue_push

.switch_stack:
    ; Switch stack
    mov     eax, [iret_data + context_t.i_ds]
    mov     ss, eax
    mov     esp, [iret_data + context_t.i_esp]

.continue_push:
    ; push the rest of iret regs
    push    dword [iret_data + context_t.i_eflags]
    push    dword [iret_data + context_t.i_cs]
    push    dword [iret_data + context_t.i_eip]

    ; set general-purpose registers
    mov     edi, [iret_data + context_t.i_edi]
    mov     esi, [iret_data + context_t.i_esi]
    mov     edx, [iret_data + context_t.i_edx]
    mov     ecx, [iret_data + context_t.i_ecx]
    mov     ebx, [iret_data + context_t.i_ebx]
    mov     eax, [iret_data + context_t.i_eax]
    mov     ebp, [iret_data + context_t.i_ebp]
    
    ; perform iret
    iret

section .bss
    iret_data: resb context_t_size



