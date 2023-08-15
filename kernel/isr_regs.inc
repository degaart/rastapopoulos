; vim: set ft=nasm:
struc isr_regs
    .ds:        resd 1
    .edi:       resd 1
    .esi:       resd 1
    .ebp:       resd 1
    .unused:    resd 1
    .ebx:       resd 1
    .edx:       resd 1
    .ecx:       resd 1
    .eax:       resd 1
    .int_no:    resd 1
    .err_code:  resd 1
    .eip:       resd 1
    .cs:        resd 1
    .eflags:    resd 1
    .esp:       resd 1
    .ss:        resd 1
    .v86_es:    resd 1
    .v86_ds:    resd 1
    .v86_fs:    resd 1
    .v86_gs:    resd 1
endstruc

