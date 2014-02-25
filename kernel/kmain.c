/*
	RastaPopoulOS
	A kernel with proper terminal output
*/
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "kutil.h"
#include "kterm.h"
#include "kidt.h"
#include "kstub.h"
#include "pic.h"
#include "pit.h"
#include "kstring.h"
#include "kmalloc.h"
#include "pmm.h"
#include "vmm.h"
#include "kmalloc.h"
#include "ll.h"
#include "gdt.h"

static uint8_t usermode_stack[65536];

void infinite_recurse() {
	infinite_recurse();
}

void irq0_handler(uint32_t irq) {
	static uint32_t counter = 0;
	counter++;
	/*if((counter % 100)==0) {
	}*/
}

/*
	Using some linker magic (cf kernel.ld), this variable
	lies on the end of the bss section, i.e.
	on the end of the kernel
	Taking it's address brings us the physical address of the
	end of the kernel
*/
extern uint32_t _kernel_end;
void kmain() {
	/* Init initial kernel terminal handling */
	term_init();
	write_string_attr("RastapopoulOS", COLOR_CYAN);
	write_string_attr(" started\n", COLOR_LIGHT_GREY);

	/* Get end of kernel memory */
	kernel_end = ALIGN(&kernel_end, 4096);
	
	/* Install new GDT */
	gdt_init();

	/* Load IDT */
	write_string("Loading IDT\n");
	idt_setup();

	/* Initialize VMM*/	
	write_string("Initializing VMM\n");
	vmm_dump_mem_regions();
	vmm_init();

	/* Initialize kernel allocator */
	write_string("Initializing kernel allocator\n");
	kmalloc_init();
	write_format("Free physical memory: %u Kb\n", pmm_get_free()/1024);

	/* Init IRQs */
	write_string("Remapping IRQs\n");
	pic_remap(IDT_IRQ_START, IDT_IRQ_START+8);
	
	write_string("Masking unused IRQs\n");
	pic_disable();
/* 	pic_enable_line(0); */

	/* Test: exec code in user-mode */
	write_string("Calling user-mode\n");
	_call_usermode(
		USER_DATA_SEL|0x3,
		(uint32_t)usermode_stack,
		USER_CODE_SEL|0x3,
		(uint32_t)_usermode_entry
	);
	_halt();

	/* Initializing system clock */	
	write_string("Adding handler for IRQ0\n");
	idt_set_irq_handler(0, irq0_handler);
	
	write_string("Setting PIT interval to 100hz\n");
	pit_set_interval(100);

	/* Ready to enable interrupts */
	write_string("Enabling interrupts\n");
	sti();

	/* Halt */
	write_string("Halting\n");
	while(1)
		_ihalt();
}

