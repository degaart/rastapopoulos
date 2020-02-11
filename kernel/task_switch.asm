;
; switch to another task (which is running in kernel mode)
;

section .text

extern current_task
extern __log
extern abort

; void task_switch(struct task* next);
; interrupts are assumed to be disabled
global task_switch:function
task_switch:
    push    ebp
    mov     ebp, esp

    push    ebx
    push    esi
    push    edi

    pushf
    pop     eax
    and     eax, 0x200
    test    eax, eax
    jnz     .stupid

    ; save old esp into tcb
    mov     esi, [current_task]
    mov     [esi], esp

    ; load next task's state
    ; TODO: TSS

    ; esp
    mov     ebx, [ebp+8]
    mov     [current_task], ebx
    mov     eax, [ebx]
    mov     esp, eax

    mov     eax, [ebx+4]
    mov     ecx, cr3
    cmp     eax, ecx
    je      .nocr3
    mov     cr3, eax

.nocr3:
    pop     edi
    pop     esi
    pop     ebx
    pop     ebp
    ret

.stupid:
    push    message
    push    0
    push    file
    push    func
    call    __log
    call    abort

section .rodata
message: db "*** KERNEL PANIC ***", 10
         db "I told you disable interrupts, stupid!", 0
file: db "task_switch.asm", 0
func: db "task_switch", 0

