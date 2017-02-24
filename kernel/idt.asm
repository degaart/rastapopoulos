section .text

global lidt
lidt:
    mov     eax, [esp+4]  ; Get the pointer to the idt_ptr, passed as a parameter.
    lidt    [eax]        ; Load the IDT pointer.
    ret

%macro ISR_DEBUG 1
    isr_stub_%1:
        mov     [0x000B8000], BYTE %1
        mov     [0x000B8001], BYTE 0x28
        cli
        hlt
%endmacro

%macro ISR_NOERRCODE 1  ; define a macro, taking one parameter
    isr_stub_%1:
        push    byte 0
        push    dword %1
        jmp     isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
    isr_stub_%1:
        push    dword %1
        jmp     isr_common_stub
%endmacro

;
; Special case for abort class exceptions
; Disable interrupts, paging, and setup a dedicated stack
;
%macro ISR_ABORT 1
    isr_stub_%1:
        ; Disable interrupts
        cli

        ; Disable paging
        mov     eax, cr0
        and     eax, 0xFFFFFFFF & ~0x80000000 ; The mask is there to get rid of nasm spurious warning
        mov     cr0, eax

        ; Setup dedicated stack
        mov     eax, 0x7CFF
        mov     esp, eax

        ; Print error message to debug port
        xor     eax, eax
        mov     dx, 0xE9
        mov     esi, .msg

        .loop:
            mov     al, [esi]
            test    al, al
            jz      .halt
            out     dx, al
            inc     esi
            jmp     .loop

    .halt:
        jmp .halt

    .msg: db "Abort-class interrupt. System halted", 10, 0

%endmacro

; This is our common ISR stub. It saves the processor state, sets
; up for kernel mode segments, calls the C-level fault handler,
; and finally restores the stack frame.
extern isr_handler
isr_common_stub:
    pusha                       ; Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax

    xor     eax, eax
    mov     ax, ds              ; Lower 16-bits of eax = ds.
    push    eax                 ; save the data segment descriptor

    mov     ax, 0x10            ; load the kernel data segment descriptor
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax

    call    isr_handler

    pop     eax                 ; reload the original data segment descriptor
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
    %if ((isr_index>=10) && (isr_index<=14)) || (isr_index==17)
        ISR_ERRCODE isr_index
    %elif isr_index==8
        ISR_ABORT isr_index
    %else
        ISR_NOERRCODE isr_index
    %endif
    ;ISR_DEBUG isr_index

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




