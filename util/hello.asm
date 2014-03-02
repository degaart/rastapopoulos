bits 32
org 0x8000000


start:
		xchg bx, bx
		mov eax, 0xB16B00B5
		int 0x80


halt:
		jmp halt
		
	

