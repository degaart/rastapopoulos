#ifndef _KIDT_H_
#define _KIDT_H_

	struct IDTR {
		uint16_t limit; 	/* IDT length in bytes, - 1 */
		uint32_t base;		/* Linear address of IDT */
	} __attribute__((packed));
	
	struct IDT_ENTRY {
		uint16_t offset_lo;		/* Offset, low-order word */
		uint16_t selector;		/* Code segment selector */
		uint8_t reserved;		/* Always 0 */
		uint8_t attributes;		/* Attributes & type */
		uint16_t offset_hi;		/* Offset, high-order word */
	} __attribute__((packed));
	
	#define IDT_ATTR_PRESENT(x)		(((x) & 0x1) << 7)
	#define IDT_ATTR_PRIVILEGE(x)	(((x) & 0x3) << 5)
	#define IDT_ATTR_STORAGE_SEG(x)	(((x) & 0x1) << 4)
	#define IDT_ATTR_TYPE(x)		((x) & 0xF)
	
	#define IDT_GATE_TASK32			0x05
	#define IDT_GATE_INT32			0x0E
	#define IDT_GATE_TRAP32			0x0F
	
	#define IDT_GATE_INT16			0x06
	#define IDT_GATE_TRAP16			0x07
	
	void idt_setup();
	
#endif //_KIDT_H_

