#include "idt.h"
#include "gdt.h"
#include "string.h"
#include "registers.h"
#include "debug.h"

#define IDT_PRESENT        (1 << 7)
#define IDT_DPL0           (0)
#define IDT_DPL1           (1 << 5)
#define IDT_DPL2           (2 << 5)
#define IDT_DPL3           (3 << 5)
#define IDT_TASK_GATE      (5)
#define IDT_TSS_32_AVL     (9)
#define IDT_TSS_32_BUSY    (11)
#define IDT_INT_GATE_32    (14)
#define IDT_TRAP_GATE_32   (15)

struct idt_entry {
    uint16_t offset_lowerbits;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_higherbits;
};

struct idt_ptr {
    uint16_t limit;
    struct idt_entry* base;
};

static struct idt_entry idt_entries[256];
static isr_handler_t isr_handlers[256];
extern uint32_t isr_stub_table[];

void idt_init()
{
    bzero(idt_entries, sizeof(idt_entries));
    bzero(isr_handlers, sizeof(isr_handlers));
    for(int i = 0; i < sizeof(idt_entries) / sizeof(idt_entries[0]); i++) {
        idt_entries[i].offset_lowerbits = isr_stub_table[i] & 0xffff;
        idt_entries[i].offset_higherbits = (isr_stub_table[i] >> 16) & 0xffff;
        idt_entries[i].selector = KERNEL_CODE_SEG;
        idt_entries[i].zero = 0;
        idt_entries[i].type_attr = IDT_PRESENT|IDT_DPL0|IDT_INT_GATE_32;
    }
    lidt(idt_entries, sizeof(idt_entries) - 1);
}

void isr_handler(const struct isr_regs* regs)
{
    if(isr_handlers[regs->int_no]) {
        isr_handlers[regs->int_no](regs);
    } else {
        trace("Unhandled int 0x%lX:\n"
              "ds: 0x%X cs: 0x%X eip: 0x%X eflags: 0x%X\n"
              "eax: 0x%X ebx: 0x%X ecx: 0x%X edx: 0x%X ebp: 0x%X esi: 0x%X edi: 0x%X\n"
              "error: 0x%X",
              regs->int_no,
              regs->ds, regs->cs, regs->eip, regs->eflags,
              regs->eax, regs->ebx, regs->ecx, regs->edx, regs->ebp, regs->esi, regs->edi,
              regs->err_code);
    }
}

void idt_install(int interrupt, isr_handler_t handler, bool usermode)
{
    if(interrupt < 0 || interrupt > 255)
        panic("Invalid interrupt number: 0x%X", interrupt);
    isr_handlers[interrupt] = handler;
    if(usermode)
        idt_entries[interrupt].type_attr |= IDT_DPL3;
    else
        idt_entries[interrupt].type_attr &= ~IDT_DPL3;
    lidt(idt_entries, sizeof(idt_entries) - 1);
}
