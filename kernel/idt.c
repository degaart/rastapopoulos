#include "idt.h"
#include "string.h"
#include "gdt.h"
#include "debug.h"
#include "kernel.h"

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

// A struct describing an interrupt gate.
struct idt_entry {
   uint16_t base_lo;             // The lower 16 bits of the address to jump to when this interrupt fires.
   uint16_t sel;                 // Kernel segment selector.
   uint8_t  always0;             // This must always be zero.
   uint8_t  flags;               // More flags. See documentation.
   uint16_t base_hi;             // The upper 16 bits of the address to jump to.
} __attribute__((packed));

// A struct describing a pointer to an array of interrupt handlers.
// This is in a format suitable for giving to 'lidt'.
struct idt_ptr {
   uint16_t limit;
   uint32_t base;                // The address of the first element in our idt_entry_t array.
} __attribute__((packed));

static struct idt_entry     idt_entries[256];
static struct idt_ptr       idt_ptr;
static isr_handler_t        isr_handlers[256];
extern uint32_t             isr_stub_table[];           /* Yes, this is an array, not a pointer */

static void set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
extern void lidt(struct idt_ptr*);

void idt_init()
{
    idt_ptr.limit = sizeof(struct idt_entry) * 256 -1;
    idt_ptr.base  = (uint32_t)&idt_entries;

    bzero(isr_handlers, sizeof(isr_handler_t));
    bzero(idt_entries, sizeof(idt_entries));
    for(int i=0; i<256; i++) {
        set_gate(i, isr_stub_table[i], KERNEL_CODE_SEG, IDT_PRESENT|IDT_DPL0|IDT_INT_GATE_32);
    }
}

void idt_flush()
{
    lidt(&idt_ptr);
}

static void set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags)
{
    idt_entries[num].base_lo = base & 0xFFFF;
    idt_entries[num].base_hi = (base >> 16) & 0xFFFF;

    idt_entries[num].sel     = sel;
    idt_entries[num].always0 = 0;

    idt_entries[num].flags   = flags;
}

void isr_handler(struct isr_regs regs)
{
    if(isr_handlers[regs.int_no]) {
        isr_handlers[regs.int_no](&regs);
    } else {
        trace(
            "Unhandled interrupt: %d\n"
            "\tds:  0x%X\n"
            "\teax: 0x%X ebx: 0x%X ecx: 0x%X edx: 0x%X\n"
            "\tesi: 0x%X edi: 0x%X\n"
            "\terr: 0x%X\n"
            "\tcs:  0x%X eip: 0x%X eflags: 0x%X\n"
            "\tss:  0x%X esp: 0x%X\n",
            regs.int_no,
            regs.ds,
            regs.eax, regs.ebx, regs.ecx, regs.edx,
            regs.esi, regs.edi,
            regs.err_code,
            regs.cs, regs.eip, regs.eflags, 
            regs.ss, regs.esp
        );
        halt();
    }
}

void idt_install(int num, isr_handler_t handler, bool usermode)
{
    isr_handlers[num] = handler;
    if(usermode)
        idt_entries[num].flags |= IDT_DPL3;
    else
        idt_entries[num].flags &= ~IDT_DPL3;
}


