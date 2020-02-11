;
; switch to another task (which is running in kernel mode)
;

section .text
extern current_task

; void task_switch(struct task* next);
; interrupts are assumed to be disabled
global task_switch:function
task_switch:
    xchg    bx, bx
    push    ebp
    mov     ebp, esp

    push    ebx
    push    esi
    push    edi

    ; save old esp into tcb
    mov     esi, [current_task]
    mov     [esi], esp

    ; load next task's state
    ; TODO: TSS, cr3
    mov     ebx, [ebp+8]
    mov     [current_task], ebx
    mov     eax, [ebx]
    mov     esp, eax
    
    pop     edi
    pop     esi
    pop     ebx
    pop     ebp
    ret

