write_string:
		; write string at current cursor position and advance cursor
		; Params: string to print
		push bp
		mov bp, sp
		push si
		
		; put string pointer in si
		mov si, [bp+4]
	.loop:
		mov al, [si]
		cmp al, 0
		jz	.end_print
		
		mov ah, 0x0E
		mov al, [si]
		mov bh, 0
		int 0x10
		inc si
		jmp .loop
	.end_print:
		pop si
		pop bp
		ret 2

%define _param0 bp+4    ; word
%define _param1 bp+6    ; word
%define _param2 bp+8    ; word
memcmp:
		; Compare two buffers
		; Params:
		;	buffer1
		; 	buffer2
		;	length (word)
		push bp
		mov bp, sp
		
		push si
		push di
		push bx
		
		mov si, [_param0]
		mov di, [_param1]
		mov bx, [_param2]
		
	.loop:
		cmp bx, 0
		jz	.equals
		
		mov al, [si+bx]
		cmp al, [di+bx]
		jne .notequals
		
		inc si
		inc di
		dec bx
		jmp .loop
	.equals:
		mov ax, 0
		jmp .return
	.notequals:
		mov ax, 1
	.return:
		pop bx
		pop di
		pop si
		pop bp
		ret 6
