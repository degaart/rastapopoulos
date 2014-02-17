#ifndef _KIDT_H_
#define _KIDT_H_
	
	typedef void (*ISR_PROC)(uint32_t, uint32_t, const void* esp);
	typedef void (*IRQ_HANDLER)(uint32_t);

	struct IDT_ENTRY {
		ISR_PROC handler;
		uint16_t attributes;
		uint16_t selector;
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
	void idt_map_default_irqs();
	void idt_set_irq_handler(int irq, IRQ_HANDLER handler);
	
#endif //_KIDT_H_

