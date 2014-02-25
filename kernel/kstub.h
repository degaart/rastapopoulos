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
	
	extern void _write_cr0(uint32_t);
	extern uint32_t _read_cr0();
	extern void _write_cr2(uint32_t);
	extern uint32_t _read_cr2();
	extern void _write_cr3(uint32_t);
	extern uint32_t _read_cr3();
	extern void _write_cr4(uint32_t);
	extern uint32_t _read_cr4();

	struct IDT_ENTRY;
	extern void _idt_load(const struct IDT_ENTRY*, uint32_t count);
	
	struct GDT_ENTRY;
	extern void _gdt_load(const struct GDT_ENTRY* gdt, uint32_t size);
	
	extern void _tss_load(uint32_t tss_selector);
	
	extern void _call_usermode(uint32_t ss, uint32_t esp, uint32_t cs, uint32_t eip);
	extern void _usermode_entry();
	
#endif //_KSTUB_H_

