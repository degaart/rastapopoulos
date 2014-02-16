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

void kmain() {
	term_init();
	write_string_attr("RastapopoulOS", COLOR_CYAN);
	write_string_attr(" started\n", COLOR_LIGHT_GREY);

	write_string("Loading IDT\n");
	idt_setup();
	write_string("IDT loaded\n");
	
	write_string("Calling INT80\n");
	_int80();
}
