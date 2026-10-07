section .text

%macro ISR_NOERRCODE 1  ; define a macro, taking one parameter
    isr_stub_%1:
        push    dword 0             ; error code
        push    dword %1            ; isr number
        jmp     isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
    isr_stub_%1:
        push    dword %1            ; isr number
        jmp     isr_common_stub
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

    ; if there is no privilege change, ss is unchanged
    ; if there is a privilege change, ss:esp is loaded from tss
    mov     ebx, esp            ; save esp
    and     esp, -4             ; align to 4 bytes
    push    ebx                 ; arg=value of esp (before alignment)
    call    isr_handler
    mov     esp, ebx            ; restore esp, discard args

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
    %if isr_index==8 || isr_index==10 || isr_index==11 || isr_index==12 || isr_index==13 || isr_index==14 || isr_index==17 || isr_index==30
        ISR_ERRCODE isr_index
    %else
        ISR_NOERRCODE isr_index
    %endif

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

