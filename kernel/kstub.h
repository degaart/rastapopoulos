#ifndef _KSTUB_H_
#define _KSTUB_H_

	extern void _outb(unsigned port, unsigned val);
	extern uint8_t _inb(unsigned port);
	extern void _breakpoint();
	extern void _delay();

#endif //_KSTUB_H_

