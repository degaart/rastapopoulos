section .text

global _start
_start:
    ; clear bss 
    extern __bss_start
    extern __bss_end
    lea rdi, [rel __bss_start]
    lea rcx, [rel __bss_end]
    sub rcx, rdi

    xor eax, eax
    cld
    rep stosb

    ; no need for stack alignment:
    ; rsp is 16-byte aligned at process entry
    ; CALL pushes an 8-byte return address, giving the necessary alignment
    extern main
    call main

    ; exit syscall
    ;   rax = 60
    ;   rdi = status
    mov edi, eax
    mov eax, 60
    syscall

    ; unreachable
    ud2


