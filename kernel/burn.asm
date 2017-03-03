section .text

; burn cpu cycles by executing pipeline-hostile code
global burn_cpu_cycles
burn_cpu_cycles:
    mov     ecx, [esp + 4]

.loop:
    cmp     ecx, 0
    jz      .break

%rep 256
    xor     eax, edx
    xor     edx, eax
%endrep

    dec     ecx

.break:
    ret




