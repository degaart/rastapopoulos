#include <stdint.h>
#include "kstring.h"
#include "kidt.h"
#include "kstub.h"
#include "kterm.h"
#include "pic.h"

#define KERN_CODE_SEL 0x08
#define IDT_IRQ_START	(32)
#define IDT_IRQ_END		(32+16)

static IRQ_HANDLER irq_handlers[16];

/*
	Generic handler for unhandled interrupts
*/
#define _DEF_NAME(num, nam) case num: int_name=nam; break
#define ERR_COL (COLOR_LIGHT_RED|0x10)
static void unhandled_isr(uint32_t type, uint32_t code, const void* esp) {
	const char* int_name;
	switch(type) {
	_DEF_NAME(0, "DIVIDE_ERROR");
	_DEF_NAME(2, "NMI");
	_DEF_NAME(3, "BREAKPOINT");
	_DEF_NAME(4, "OVERFLOW");
	_DEF_NAME(5, "OUTBOUND");
	_DEF_NAME(6, "INVALID_OPCODE");
	_DEF_NAME(7, "NO_MATH_COPROCESSOR");
	_DEF_NAME(8, "DOUBLE_FAULT");
	_DEF_NAME(9, "COPROCESSOR_SEG_OVERRUN");
	_DEF_NAME(10, "INVALID_TSS");
	_DEF_NAME(11, "SEGMENT_NOT_PRESENT");
	_DEF_NAME(12, "SS_FAULT");
	_DEF_NAME(13, "GENERAL_PROTECTION_FAULT");
	_DEF_NAME(14, "PAGE_FAULT");
	_DEF_NAME(16, "MATH_FAULT");
	_DEF_NAME(17, "ALIGNMENT_CHECK");
	_DEF_NAME(18, "MACHINE_CHECK");
	_DEF_NAME(19, "SIMD_FP_EXCEPTION");
	_DEF_NAME(20, "VE_EXCEPTION");
	default:
		int_name = 0;
	};

	write_string_attr("Unhandled kernel interrupt: ", ERR_COL);

	if(int_name)
		write_string_attr(int_name,ERR_COL);
	else
		write_value_attr(type, ERR_COL);

	write_string_attr(", code: ", ERR_COL);
	write_value_attr(code, ERR_COL);
	
	write_string_attr(", address: ", ERR_COL);
	write_value_attr((uint32_t)esp, ERR_COL);
	_halt();
}

static void irq_isr(uint32_t type, uint32_t code, const void* esp) {
	/* Check spurious IRQs */
	unsigned irq = type-IDT_IRQ_START;
	unsigned spurious = 0;
	
	if(irq == 7) {
		unsigned isr = pic_get_isr();
		if(!( isr & (1<<7) ))
			spurious=1;
	} else if(irq==15) {
		unsigned isr = pic_get_isr();
		if(!( isr & (1<<15) )) {
			spurious=1;
			
			/* Send non-specific EOI to master */
			pic_send_eoi(2);
		}
	}
	if(!spurious) {
		if(irq_handlers[irq])
			irq_handlers[irq](irq);
		pic_send_eoi(irq);
	}
}

void idt_setup() {
	struct IDT_ENTRY idt[48];
	
	bzero(idt, sizeof(idt));
	
	/* Use default handler for exceptions/traps/faults */
	for(int i=0; i<sizeof(idt)/sizeof(*idt); i++) {
		if((i!=1) && (i!=15)) {
			if((i>=IDT_IRQ_START) && (i<=IDT_IRQ_END)) {
				/* IRQ handler */
				idt[i].handler = irq_isr;
			} else {
				/* Interrupt handler */
				idt[i].handler = unhandled_isr;
			}
			idt[i].attributes =
				IDT_ATTR_PRESENT(1)|
				IDT_ATTR_PRIVILEGE(0)|
				IDT_ATTR_STORAGE_SEG(0)|
				IDT_GATE_INT32;
			idt[i].selector = KERN_CODE_SEL;
		}
	}

	/* Load IDT */
	_idt_load(idt, sizeof(idt)/sizeof(*idt));
}

void idt_set_irq_handler(int irq, IRQ_HANDLER handler) {
	if(_getflags() & EFLAGS_IF)
		PANIC("Attempted to set an IRQ handler while interrupts enabled");
	irq_handlers[irq] = handler;
}


