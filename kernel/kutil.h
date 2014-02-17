#ifndef _KUTIL_H_
#define _KUTIL_H_

	/*
		Kernel utility functions
		Functions are defined in kmain.c
	*/
	#define breakpoint() asm __volatile__("xchgw %%bx,%%bx;"::)
	#define cli() asm __volatile__("cli")
	#define sti() asm __volatile__("sti")
	#define pushf() asm __volatile__("pushf")
	#define popf() asm __volatile__("popf");
	
	void panic(const char* file, int line, const char* message);
	#define PANIC(msg) panic(__FILE__, __LINE__, msg);
	
	void iowait();
	
	#define EFLAGS_IF (1<<9)
	
	#define MAKEWORD(lo,hi) (((lo) & 0xFF) | (((hi) & 0xFF) << 8))
	#define MAKEDWORD(lo,hi) (((lo) & 0xFFFF)|(((hi) & 0xFFFF) << 16))

	#define LOBYTE(u)	((u) &  0x000000FF)
	#define HIBYTE(u)	(((u) & 0x0000FF00)>>8)
	#define LOWORD(u)	((u) &  0x0000FFFF)
	#define HIWORD(u)	(((u) & 0xFFFF0000)>>16)
	
	#define ALIGN(pointer,alignment) \
		((void*)((( (uintptr_t)pointer )+( alignment )-1) & ~( (alignment)-1 )))

	#include "kstub.h"

	/*
		This variable points to end of kernel memory, aligned to 4096 bytes
	*/	
	void* kernel_end;

#endif //_KUTIL_H_
