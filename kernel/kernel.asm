; Test kernel

bits 16
org 0x100000
	jmp _start
	
	%include 'string.inc'
	
_start:
		push str.hello
		call write_string
		jmp halt
halt:
		cli
		hlt
		jmp halt

str:
	.hello: db 'Hello from kernel', 0
