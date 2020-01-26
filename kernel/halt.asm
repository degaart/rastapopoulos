section .text

global halt:function (halt.end - halt)

halt:
    cli
.hang:
    hlt
    jmp .hang
.end:

