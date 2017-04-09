section .text

; void real_rdtsc(uint32_t lo, uint32_t hi)
global real_rdtsc
real_rdtsc:
    rdtsc
    mov     [esp + 4], eax
    mov     [esp + 8], edx
    ret

