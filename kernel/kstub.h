#ifndef _KSTUB_H_
#define _KSTUB_H_

	/* Functions defined in kstub.asm */
	extern void _outb(unsigned port, unsigned val);
	extern uint8_t _inb(unsigned port);
	extern void _breakpoint();
	extern void _halt();
	
	extern void _isr80();
	extern void _int80();
	extern void _cli();
	extern void _sti();
	extern void _ihalt();
	extern uint32_t _getflags();

	struct IDT_ENTRY;
	extern void _idt_load(const struct IDT_ENTRY*, uint32_t count);
	
#endif //_KSTUB_H_
