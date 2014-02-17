#ifndef _KSTUB_H_
#define _KSTUB_H_

	extern void _outb(unsigned port, unsigned val);
	extern uint8_t _inb(unsigned port);
	extern void _breakpoint();
	extern void _halt();
	
	extern void _isr80();
	extern void _int80();
	extern void _cli();
	extern void _sti();
	extern void _ihalt();

	struct IDT_ENTRY;
	extern void _idt_load(const struct IDT_ENTRY*, uint32_t count);

	#define breakpoint() asm __volatile__("xchgw %%bx,%%bx;"::)
	#define cli() asm __volatile__("cli;")
	#define sti() asm __volatile__("sti;")
	
	void panic(const char* file, int line, const char* message);
	#define PANIC(msg) panic(__FILE__, __LINE__, msg);
	
	void iowait();
	
	#define EFLAGS_IF (1<<9)
	extern uint32_t _getflags();
	
#endif //_KSTUB_H_



