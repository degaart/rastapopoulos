#include <stdint.h>
#include "kstub.h"
#include "ports.h"
#include "pic.h"
#include "kterm.h"

#define ICW1_ICW4			0x01		/* ICW4 (not) needed */
#define ICW1_SINGLE			0x02		/* Single (cascade) mode */
#define ICW1_INTERVAL4		0x04		/* Call address interval 4 (8) */
#define ICW1_LEVEL			0x08		/* Level triggered (edge) mode */
#define ICW1_INIT			0x10		/* Initialization - required! */
 
#define ICW4_8086			0x01		/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO			0x02		/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE		0x08		/* Buffered mode/slave */
#define ICW4_BUF_MASTER		0x0C		/* Buffered mode/master */
#define ICW4_SFNM			0x10		/* Special fully nested (not) */

#define PIC_EOI				0x20
#define PIC_EOI_CASCADE		0x67

#define PIC_READ_IRR		0x0a		/* OCW3 irq ready next CMD read */
#define PIC_READ_ISR        0x0b		/* OCW3 irq service next CMD read */

void pic_remap(int offset_master, int offset_slave) {
	if( (offset_master % 0x8) || (offset_slave % 0x8) )
		PANIC("Invalid offset(s) for IRQ remapping");
	
	/* Save IRQ masks */
	uint8_t mask_master = _inb(PORT_PIC1_DATA);
	uint8_t mask_slave = _inb(PORT_PIC2_DATA);
	
	/* Reinitialize */
	_outb(PORT_PIC1_COMMAND, ICW1_INIT|ICW1_ICW4);
	IOWAIT();
	_outb(PORT_PIC2_COMMAND, ICW1_INIT|ICW1_ICW4);
	IOWAIT();
	_outb(PORT_PIC1_DATA, offset_master);
	IOWAIT();
	_outb(PORT_PIC2_DATA, offset_slave);
	IOWAIT();
	_outb(PORT_PIC1_DATA, 4);		/* Presence of slave PIC */
	IOWAIT();
	_outb(PORT_PIC2_DATA, 2);		/* Slave PIC cascade identity */
	IOWAIT();
	
	_outb(PORT_PIC1_DATA, ICW4_8086);
	IOWAIT();
	_outb(PORT_PIC2_DATA, ICW4_8086);
	IOWAIT();
	
	/* Restore IRQ masks */
	_outb(PORT_PIC1_DATA, mask_master);
	_outb(PORT_PIC2_DATA, mask_slave);
}

/*
	Send end of interrupt to PIC
*/
void pic_send_eoi(uint8_t irq) {
	if(irq >= 8)
		_outb(PORT_PIC2_COMMAND, PIC_EOI);
	_outb(PORT_PIC1_COMMAND, PIC_EOI);
}

/*
	Disable teh PICs
*/
void pic_disable() {
	_outb(PORT_PIC1_DATA, 0xFF);
	_outb(PORT_PIC2_DATA, 0xFF);
}

void pic_disable_line(uint8_t line) {
	uint16_t port;
	uint8_t value;
	
	if(line < 8)
		port = PORT_PIC1_DATA;
	else {
		port = PORT_PIC2_DATA;
		line -= 8;
	}
	value = _inb(port) | (1 << line);
	_outb(port, value);
}

void pic_enable_line(uint8_t line) {
	uint16_t port;
	uint8_t value;
	
	if(line < 8)
		port = PORT_PIC1_DATA;
	else {
		port = PORT_PIC2_DATA;
		line -= 8;
	}
	value = _inb(port) & (~(1 << line));
	_outb(port, value);
}

void pic_write_mask(uint8_t pic1_mask, uint8_t pic2_mask) {
	_outb(PORT_PIC1_DATA, pic1_mask);
	_outb(PORT_PIC2_DATA, pic2_mask);
}

/* Helper func */
static uint16_t pic_get_irq_reg(int ocw3) {
    /* OCW3 to PIC CMD to get the register values.  PIC2 is chained, and
     * represents IRQs 8-15.  PIC1 is IRQs 0-7, with 2 being the chain */
    _outb(PORT_PIC1_COMMAND, ocw3);
    _outb(PORT_PIC1_COMMAND, ocw3);
    return (_inb(PORT_PIC2_COMMAND) << 8) | _inb(PORT_PIC1_COMMAND);
}
 
/*
	Returns the combined value of the cascaded PICs irq request register
	IRR register: Mask of the current interrupts that are pending acknowledgement
*/
uint16_t pic_get_irr(void) {
    return pic_get_irq_reg(PIC_READ_IRR);
}
 
/*
	Returns the combined value of the cascaded PICs in-service register
	ISR: Mask of the interrupts that are pending an EOI
*/
uint16_t pic_get_isr(void) {
    return pic_get_irq_reg(PIC_READ_ISR);
}

void pic_read_mask(uint8_t* pic1_mask, uint8_t* pic2_mask) {
	*pic1_mask = _inb(PORT_PIC1_DATA);
	*pic2_mask = _inb(PORT_PIC2_DATA);
}


