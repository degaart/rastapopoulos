;
; RastapopoulOS
; Calls C++ objects global constructors
; TODO: Reimplement in C
;

use32

extern _CTORS_START_
extern _CTORS_END_

global call_ctors
call_ctors:
    push ebp
    mov ebp, esp

    ; _CTORS_START_ contains a dword pointing to a
    ; procedure to call
    mov edx, _CTORS_START_
    
.loop:
    cmp edx, _CTORS_END_
    je .return

    mov eax, [edx]
    call eax

.return:
    pop ebp
    ret
