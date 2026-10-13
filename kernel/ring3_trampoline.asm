section .text

; This code will be copied to address 0x1000 process, then mapped
; executable only, DPL3
; It serves as a trampoline to call usermode code, then call
; syscall_exit when the usermode code returns
global ring3_trampoline
ring3_trampoline:
    ; Note: call 0x400000 does not work, nasm emits it as a relative
    ; address, which will not work because this code is copied to
    ; another location
    mov  eax, 0x400000          ; usermode entry
    call eax                    ; absolute call
    mov  eax, 3                 ; SYSCALL_EXIT
    int  0x80

    ; This should not be reachable
    int3

global ring3_trampoline_end
ring3_trampoline_end:

