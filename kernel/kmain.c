/*
	RastaPopoulOS
	A kernel with proper terminal output
*/
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "kterm.h"
#include "kidt.h"
#include "kstub.h"
#include "pic.h"

#define ERR_COL (COLOR_LIGHT_RED|0x10)

void panic(const char* file, int line, const char* message) {
	write_string_attr("Kernel error at ", ERR_COL);
	write_string_attr(file, ERR_COL);
	write_string_attr("[", ERR_COL);
	write_value_attr(line, ERR_COL);
	write_string_attr("]: ", ERR_COL);
	write_string_attr(message, ERR_COL);
	_halt();
}

void irq0_handler(uint32_t irq) {
	DUMP32(irq);
}

void kmain() {
	//_sti();
	term_init();
	write_string_attr("RastapopoulOS", COLOR_CYAN);
	write_string_attr(" started\n", COLOR_LIGHT_GREY);

	write_string("Loading IDT\n");
	idt_setup();
	write_string("IDT loaded\n");
	
	write_string("Remapping IRQs\n");
	pic_remap(32, 32+8);
	
	write_string("Masking unused IRQs\n");
	pic_disable();
	pic_enable_line(0);
	
	write_string("Adding handler for IRQ0\n");
	idt_set_irq_handler(0, irq0_handler);

	write_string("Enabling interrupts\n");
	sti();
halte:
	_ihalt();
	goto halte;

	_halt();
}

#if 0
	/* Program the PIT */
	int divisor = 1193180 / 100;       /* Calculate our divisor */
    _outb(0x43, 0x36);             /* Set our command byte 0x36 */
    _outb(0x40, divisor & 0xFF);   /* Set low byte of divisor */
    _outb(0x40, divisor >> 8);     /* Set high byte of divisor */
#endif

