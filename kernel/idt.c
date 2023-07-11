#include "idt.h"
#include "debug.h"
#include <stddef.h>

struct idt_entry {
    uint16_t offset_lowerbits;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_higherbits;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    struct idt_entry* base;
} __attribute__((packed));

extern uint32_t isr_stub_table[];
static isr_t isr_handlers[256];
static struct idt_entry idt_entries[256];
static struct idt_ptr idtr __attribute__((aligned(8)));

void idt_init()
{
    for(size_t i = 0; i < sizeof(idt_entries)/sizeof(idt_entries[0]); i++) {
        idt_entries[i].offset_lowerbits = (isr_stub_table[i] & 0xFFFF);
        idt_entries[i].offset_higherbits = (isr_stub_table[i] >> 16) & 0xFFFF;
        idt_entries[i].selector = 0x08; /* kernel code segment */
        idt_entries[i].zero = 0;
        idt_entries[i].type_attr = IDT_PRESENT|IDT_DPL0|IDT_INT_GATE_32;
    }
    idtr.limit = sizeof(idt_entries) - 1;
    idtr.base = idt_entries;
    asm volatile("lidt %0" :: "m"(idtr));
}

void isr_handler(struct isr_regs* regs)
{
    if(isr_handlers[regs->int_no])
        isr_handlers[regs->int_no](regs);
    else
        PANIC("Unhandled interrupt 0x%x", regs->int_no);
}

void idt_add_handler(int number, isr_t handler, unsigned dpl)
{
    assert(number >= 0);
    assert(number < 256);
    assert(dpl == 0 || dpl == 3);
    isr_handlers[number] = handler;
    idt_entries[number].type_attr = IDT_PRESENT|IDT_INT_GATE_32|(dpl << 5);
    asm volatile("lidt %0" :: "m"(idtr));
}

