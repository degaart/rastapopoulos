; vim: set ft=nasm:
section .text

%macro ISR_NOERRCODE 1
    isr_stub_%1:
        cli
        push    dword 0             ; push fake error code
        push    dword %1
        jmp     isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
    isr_stub_%1:
        cli
        push    dword %1
        jmp     isr_common_stub
%endmacro

; This is our common ISR stub. It saves the processor state, sets
; up for kernel mode segments, calls the C-level fault handler,
; and finally restores the stack frame.
extern isr_handler
isr_common_stub:
    pusha                       ; Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax

    xor     eax, eax
    mov     ax, ds
    push    eax                 ; push ds

    mov     ax, 0x10            ; load the kernel data segment descriptor
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax

    mov     ebp, esp
    sub     esp, 4
    and     esp, 0xfffffff0     ; new gcc's require a 16-byte aligned stack
    mov     [esp], ebp          ; first argument: value of esp
    call    isr_handler
    mov     esp, ebp

    pop     eax
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax

    popa                        ; Pops edi,esi,ebp...
    add     esp, 8              ; Cleans up the pushed error code and pushed ISR number
    iret                        ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP 


; Generate the ISR thunks
%assign isr_index 0
%rep 256
    isr_stub_ %+ isr_index:
        xchg bx, bx
        cli
    %if (isr_index != 8) && \
          (isr_index != 10) && \
          (isr_index != 13) && \
          (isr_index != 14) && \
          (isr_index != 17) && \
          (isr_index != 30)
        push    dword 0             ; push fake error code
    %endif
    .continue:
        push    dword isr_index
        jmp     isr_common_stub

    %assign isr_index isr_index+1
%endrep

%macro ISR_STUB_TABLE_ENTRY 1
    dd isr_stub_%1
%endmacro

; Table which stores addresses of isr stubs
section .rodata
global isr_stub_table
isr_stub_table:
    %assign isr_index 0
    %rep 256
        ISR_STUB_TABLE_ENTRY isr_index
        %assign isr_index isr_index+1
    %endrep

