#include "pic.h"
#include "debug.h"
#include "io.h"
#include "idt.h"
#include "string.h"
#include <stddef.h>

#define PIC1		0x20		/* IO base address for master PIC */
#define PIC2		0xA0		/* IO base address for slave PIC */
#define PIC1_COMMAND	PIC1
#define PIC1_DATA	(PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA	(PIC2+1)

#define PIC_EOI		0x20		/* End-of-interrupt command code */

#define PIC1_CMD                    0x20
#define PIC2_CMD                    0xA0
#define PIC_READ_IRR                0x0a    /* OCW3 irq ready next CMD read */
#define PIC_READ_ISR                0x0b    /* OCW3 irq service next CMD read */

#define ICW1_ICW4	0x01		/* ICW4 (not) needed */
#define ICW1_SINGLE	0x02		/* Single (cascade) mode */
#define ICW1_INTERVAL4	0x04		/* Call address interval 4 (8) */
#define ICW1_LEVEL	0x08		/* Level triggered (edge) mode */
#define ICW1_INIT	0x10		/* Initialization - required! */
 
#define ICW4_8086	0x01		/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO	0x02		/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE	0x08		/* Buffered mode/slave */
#define ICW4_BUF_MASTER	0x0C		/* Buffered mode/master */
#define ICW4_SFNM	0x10		/* Special fully nested (not) */

static irq_handler_t irq_handlers[16];
static unsigned unhandled_interrupts;

static
void pic_eoi(int irq)
{
    assert(irq >= 0 && irq < 16);

	if(irq >= 8)
		outb(PIC2_COMMAND, PIC_EOI);
 
	outb(PIC1_COMMAND, PIC_EOI);
}

void irq_mask(int irq)
{
    assert(irq >= 0 && irq < 16);

    uint16_t port;
    uint8_t value;
 
    if(irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) | (1 << irq);
    outb(port, value);        
}
 
void irq_unmask(int irq)
{
    assert(irq >= 0 && irq < 16);

    uint16_t port;
    uint8_t value;
 
    if(irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) & ~(1 << irq);
    outb(port, value);        
}

/* Helper func */
static
uint16_t __pic_get_irq_reg(int ocw3)
{
    /* OCW3 to PIC CMD to get the register values.  PIC2 is chained, and
     * represents IRQs 8-15.  PIC1 is IRQs 0-7, with 2 being the chain */
    outb(PIC1_CMD, ocw3);
    outb(PIC2_CMD, ocw3);
    return (inb(PIC2_CMD) << 8) | inb(PIC1_CMD);
}
 
/*
 * IRR: Interrupt request register
 * which interrupts have been raised
 * Note: bit 2 will always be set for pic2 (because it's cascaded)
 */
static
uint16_t pic_get_irr(void)
{
    return __pic_get_irq_reg(PIC_READ_IRR);
}
 
/*
 * ISR: Interrupt service register
 * which interrupts are being serviced, meaning IRQs sent to the CPU
 * Note: bit 2 will always be set for pic2 (because it's cascaded)
 */
static
uint16_t pic_get_isr(void)
{
    return __pic_get_irq_reg(PIC_READ_ISR);
}

static
void pic_remap(int offset1, int offset2)
{
	unsigned char a1, a2;
 
	a1 = inb(PIC1_DATA);                        // save masks
	a2 = inb(PIC2_DATA);
 
	outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);  // starts the initialization sequence (in cascade mode)
	io_wait();
	outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
	io_wait();
	outb(PIC1_DATA, offset1);                 // ICW2: Master PIC vector offset
	io_wait();
	outb(PIC2_DATA, offset2);                 // ICW2: Slave PIC vector offset
	io_wait();
	outb(PIC1_DATA, 4);                       // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
	io_wait();
	outb(PIC2_DATA, 2);                       // ICW3: tell Slave PIC its cascade identity (0000 0010)
	io_wait();
 
	outb(PIC1_DATA, ICW4_8086);
	io_wait();
	outb(PIC2_DATA, ICW4_8086);
	io_wait();
 
	outb(PIC1_DATA, a1);   // restore saved masks.
	outb(PIC2_DATA, a2);
}

static
void irq_stub(const struct isr_regs* regs)
{
    int irq = regs->int_no - 0x20;

    /* Handle spurious irqs */
    if(irq == 7) {
        if(!(pic_get_isr() & (1 << 7))) {
            trace("WARNING: Spurious irq7");
            return;
        }
    } else if(irq == 15) {
        if(!(pic_get_isr() & (1 << 15))) {
            trace("WARNING: Spurious irq15");
            pic_eoi(2);
            return;
        }
    }

    /*
     * Mask interrupt
     * Send eoi
     * Execute handler
     * TODO: Should unmasking the irq be done by handler?
     */
    //irq_mask(irq);
    pic_eoi(irq);
    if(irq_handlers[irq]) {
        irq_handlers[irq](irq, regs);
        //irq_unmask(irq);
    } else {
        if(!(unhandled_interrupts & (1 << irq))) {
            trace("WARNING: Unhandled irq %d", irq);
            unhandled_interrupts |= (1 << irq);
        }
    }
}

void pic_init()
{
    pic_remap(0x20, 0x28);

    /* 
     * mask all for now 
     * should IRQ_CASCADE be masked too?
     */
    for(int irq = 0; irq < 16; irq++) {
        if(irq != IRQ_CASCADE)
            irq_mask(irq);
    }

    bzero(irq_handlers, sizeof(irq_handlers));
    for(size_t i = 0x20; i < 0x31; i++)
        idt_install(i, irq_stub, false);            /* TODO: Check why the last parameter is false? */
}

void pic_install(int irq, irq_handler_t handler)
{
    assert(irq >= 0 && irq < 16);
    irq_handlers[irq] = handler;
}

void pic_remove(int irq)
{
    assert(irq >= 0 && irq < 16);
    irq_handlers[irq] = NULL;
}

