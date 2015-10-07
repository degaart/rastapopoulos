section .text

global _flush_tlb
_flush_tlb:
    mov eax, [esp+4]
    cmp eax, 0x00400000
    jne .doflush
    ;xchg bx, bx
.doflush:
    invlpg [esp+4]
    ret


