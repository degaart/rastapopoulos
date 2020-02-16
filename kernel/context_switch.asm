;
; Switch the current kernel stack and return
;

section .text
extern _current_task

;
; void context_switch(struct task* next)
; 
; switches task and save old esp
; this saving needs to happen here because upon ret, we'll
; be running inside another context
;
global context_switch:function
context_switch:
    push    ebp
    mov     ebp, esp

    ; save old esp
    mov     eax, [_current_task]
    mov     [eax], esp

    ; set current_task to next
    mov     eax, [ebp + 8]
    mov     [_current_task], eax

    ; load new esp
    mov     eax, [eax]
    mov     esp, eax

    ; done
    pop     ebp
    ret

