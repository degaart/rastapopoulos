section .text

%macro ISR_NOERRCODE 1  ; define a macro, taking one parameter
    isr_stub_%1:
        cli
        push byte 0
        push dword %1
        jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
    isr_stub_%1:
        cli
        push dword %1
        jmp isr_common_stub
%endmacro

; Special case for abort class exceptions
; Disable interrupts, paging, and setup a dedicated stack
%macro ISR_ABORT 1
    isr_stub_%1:
        cli

        mov eax, cr0
        and eax, ~0x80000000
        mov cr0, eax

        mov eax, 0x7CFF
        mov esp, eax

        mov dx, 0xE9
        mov al, 'X'
        out dx, al
    .halt:
        jmp .halt

        push word 0
        push word 0
        pushf
        push word 0
        push word 0
        push byte 0
        push dword %1

        jmp isr_common_stub
%endmacro

; This is our common ISR stub. It saves the processor state, sets
; up for kernel mode segments, calls the C-level fault handler,
; and finally restores the stack frame.
extern isr_handler
isr_common_stub:
   pusha                    ; Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax

   xor eax, eax
   mov ax, ds               ; Lower 16-bits of eax = ds.
   push eax                 ; save the data segment descriptor

   mov ax, 0x10  ; load the kernel data segment descriptor
   mov ds, ax
   mov es, ax
   mov fs, ax
   mov gs, ax

   call isr_handler

   pop eax        ; reload the original data segment descriptor
   mov ds, ax
   mov es, ax
   mov fs, ax
   mov gs, ax

   popa                     ; Pops edi,esi,ebp...
   add esp, 8     ; Cleans up the pushed error code and pushed ISR number
   sti
   iret           ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP 

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



