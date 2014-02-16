#include <stdint.h>
#include "kstring.h"
#include "kidt.h"
#include "kstub.h"

#define SELECTOR_CODE 0x08

static struct IDT_ENTRY encode_idt(uint16_t selector, uint32_t offset, uint8_t attributes) {
	struct IDT_ENTRY entry;
	
	entry.selector = selector;
	entry.offset_lo = (offset & 0x0000FFFF);
	entry.offset_hi = (offset & 0xFFFF0000) << 16;
	entry.attributes = attributes;
	return(entry);
}

void idt_setup() {
	struct IDT_ENTRY idt[256];
	struct IDTR idtr;
	
	/*
		Init unused entries
		Note: we just need to set the present bit to 0
		so bzero works
	*/
	bzero(idt, sizeof(idt));
	
	/* Add ISR for int 80 */
	idt[0x80] = encode_idt(
		SELECTOR_CODE,
		(uint32_t)_isr80,
		IDT_ATTR_PRESENT(1)|
		IDT_ATTR_PRIVILEGE(0)|
		IDT_ATTR_STORAGE_SEG(0)|
		IDT_GATE_INT32
	);

	idtr.limit = sizeof(idt) - 1;
	idtr.base = (uint32_t)idt;
	_idt_load(&idtr, idt);
	//_sti();
}

