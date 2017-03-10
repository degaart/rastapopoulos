section .text

%include "iret.inc"

global save_context
save_context:
    push        ebp
    mov         ebp, esp

    mov         ecx, [ebp + 8]

    mov         eax, cs
    mov         [ecx + iret_t.i_cs], eax

    mov         eax, ds
    mov         [ecx + iret_t.i_ds], eax

    mov         eax, ss
    mov         [ecx + iret_t.i_ss], eax

    mov         eax, cr3
    mov         [ecx + iret_t.i_cr3], eax
    
    mov         eax, ebp
    add         eax, 8 
    mov         [ecx + iret_t.i_esp], eax

    pushf
    pop         eax
    mov         [ecx + iret_t.i_eflags], eax

    mov         eax, [ebp + 4]
    mov         [ecx + iret_t.i_eip], eax

    mov         [ecx + iret_t.i_edi], edi
    mov         [ecx + iret_t.i_esi], esi
    mov         [ecx + iret_t.i_ebx], ebx
    mov         eax, [ebp + 4]
    mov         [ecx + iret_t.i_ebp], eax

    pop         ebp
    xor         eax, eax
    ret

