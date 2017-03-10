; vim: set ft=asm:

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

