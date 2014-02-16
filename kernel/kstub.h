#ifndef _KSTUB_H_
#define _KSTUB_H_

	extern void _outb(unsigned port, unsigned val);
	extern uint8_t _inb(unsigned port);
	extern void _breakpoint();
	extern void _delay();
	
	extern void _isr80();
	extern void _int80();
	extern void _cli();
	extern void _sti();

	struct IDTR;
	struct IDT_ENTRY;
	extern void _idt_load(const struct IDTR*, const struct IDT_ENTRY*);
	

	#define breakpoint() asm __volatile__("xchgw %%bx,%%bx;"::)
#endif //_KSTUB_H_

