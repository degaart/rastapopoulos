; 
; ISR entry point
; Problem: how do we know which C ISR to call?
; Fucking solution: Generate 256 ISRs
; Use a fucking static table to which the ISRs will
; look for the corresponding C function to call
; Note: the _idt_load should know when an ISR is not
; present, and instead of providing lidt with the thunk,
; should just put a zero IDT entry there
; Note: this code assumes the stack is usable
;
; Param for this macro: index into __isr_table
%macro __isr_thunk 1
align 4
__isr_thunk_%1:
		;breakpoint
		push ebp
		mov ebp, esp
		pusha
		push ds
		push es
		push fs
		push gs
		
		; setup registers
		mov ax, KERN_DATA_SEL
		mov ds, ax
		mov es, ax
		mov fs, ax
		mov gs, ax
	
		; now, onto the error code
		%if (%1==8) || ((%1>=10) && (%1<=14)) || (%1==17)
			; push fault address
			push dword [ebp+8]

			; move error code into eax
			mov eax, [ebp+4]
		%else
			; push fault address
			push dword [ebp+4]
		
			; move error code into eax
			mov eax, 0xFFFFFFFF
		%endif
		
		; push error code
		push eax
		
		; push isr index
		push dword %1
		
		; call C function
		call [__isr_table+(%1*4)];
		
		; cleanup stack
		add esp, 12

		; pop error code if needed
		%if (%1==8) || ((%1>=10) && (%1<=14)) || (%1==17)
			pop eax
		%endif
		
		; restore state
		pop gs
		pop fs
		pop es
		pop ds
		popa
		pop ebp
		iret
%endmacro

; Generate the ISR thunks
%assign isr_index 0
%rep 256
	__isr_thunk isr_index
	%assign isr_index isr_index+1
%endrep

; generate the ISR thunk table
align 4
__isr_thunk_table: 
%assign isr_index 0
%rep 256
	dd __isr_thunk_%[isr_index],
	%assign isr_index isr_index+1
%endrep
	dd 0


