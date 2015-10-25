#ifndef _PIC_H_
#define _PIC_H_

#include <stdint.h>
#include "idt.h"

class PIC {
private:
    // Master PIC - Command    0x0020
    // Master PIC - Data   0x0021
    // Slave PIC - Command     0x00A0
    // Slave PIC - Data    0x00A1
    static const int PIC0_COMMAND = 0x20;
    static const int PIC0_DATA = 0x21;
    static const int PIC1_COMMAND = 0xA0;
    static const int PIC1_DATA = 0xA1;

    static const uint8_t COMMAND_EOI = 0x20;

    static const uint8_t COMMAND_ICW1_IC4 = 0x01;               /* If set(1), the PIC expects to recieve IC4 during initialization. */
    static const uint8_t COMMAND_ICW1_SNGL = 0x02;              /* If set(1), only one PIC in system. If cleared, PIC is cascaded with slave PICs, and ICW3 must be sent to controller. */
    static const uint8_t COMMAND_ICW1_ADI = 0x04;               /*  If set (1), CALL address interval is 4, else 8 */
    static const uint8_t COMMAND_ICW1_LTIM = 0x08;              /* If set (1), Operate in Level Triggered Mode. If Not set (0), Operate in Edge Triggered Mode */
    static const uint8_t COMMAND_ICW1_INIT = 0x10;              /* Initialization */

    static const uint8_t DATA_ICW4_8086 = 0x01;              /* 8086/88 (MCS-80/85) mode */
    static const uint8_t DATA_ICW4_AUTO = 0x02;              /* Auto (normal) EOI */
    static const uint8_t DATA_CW4_BUF_SLAVE = 0x08;          /* Buffered mode/slave */
    static const uint8_t DATA_ICW4_BUF_MASTER = 0x0C;        /* Buffered mode/master */
    static const uint8_t DATA_ICW4_SFNM = 0x10;              /* Special fully nested (not) */

    static void irq_stub(isr_regs_t* regs);
    static void eoi(unsigned irq);
public:
    typedef void (*irq_handler)(int irq, const isr_regs_t* regs);

private:
    static irq_handler _irq_handlers[16];

public:
    static void init();

    static const int IRQ_TIMER = 0;
    static const int IRQ_KEYBOARD = 1;
    static const int IRQ_SERIAL1 = 4;
    static const int IRQ_SERIAL2 = 3;
    static const int IRQ_PARPORT2 = 5;
    static const int IRQ_FDC = 6;
    static const int IRQ_PARPORT1 = 7;

    static const int IRQ_CMOSTIMER = 8;
    static const int IRQ_CGARETRACE = 9;
    static const int IRQ_AUX = 12;
    static const int IRQ_FPU = 13;
    static const int IRQ_HDC = 14;
    
    static void install_irq_handler(int irq, irq_handler handler);
    static void remove_irq_handler(int irq, irq_handler handler);

};

#endif

