#include "gdt.h"
#include "debug.h"
#include "string.h"


#define GDT_ACCESSED        1
#define GDT_WRITABLE        (1 << 1)
#define GDT_DIRECTION       (1 << 2)
#define GDT_TYPE(t)         ( ( (t) & 1) << 4 )        // 0: system, 1: code/data
#define GDT_DPL(d)          ( (d & 0x3) << 5 )
#define GDT_PRESENT         (1 << 7)

#define GDT_READABLE        (1 << 1)
#define GDT_CONFORMING      (1 << 2)
#define GDT_CODE            (1 << 3)
#define GDT_DEFAULT         (1 << 6)

#define GDT_AVAIL(a)        ( ((a) & 1) << 4 )
#define GDT_16BIT           0
#define GDT_32BIT           (1 << 6)
#define GDT_GRAN1B          0
#define GDT_GRAN4K         (1 << 7)

struct tss_entry_t {
   uint32_t prev_tss;   // The previous TSS - if we used hardware task switching this would form a linked list.
   uint32_t esp0;       // The stack pointer to load when we change to kernel mode.
   uint32_t ss0;        // The stack segment to load when we change to kernel mode.
   uint32_t esp1;       // Unused...
   uint32_t ss1;
   uint32_t esp2;
   uint32_t ss2;
   uint32_t cr3;
   uint32_t eip;
   uint32_t eflags;
   uint32_t eax;
   uint32_t ecx;
   uint32_t edx;
   uint32_t ebx;
   uint32_t esp;
   uint32_t ebp;
   uint32_t esi;
   uint32_t edi;
   uint32_t es;         // The value to load into ES when we change to kernel mode.
   uint32_t cs;         // The value to load into CS when we change to kernel mode.
   uint32_t ss;         // The value to load into SS when we change to kernel mode.
   uint32_t ds;         // The value to load into DS when we change to kernel mode.
   uint32_t fs;         // The value to load into FS when we change to kernel mode.
   uint32_t gs;         // The value to load into GS when we change to kernel mode.
   uint32_t ldt;        // Unused...
   uint16_t trap;
   uint16_t iomap_base;
} __attribute__((packed));

struct gdt_entry_t {
   uint16_t limit_low;           // The lower 16 bits of the limit.
   uint16_t base_low;            // The lower 16 bits of the base.
   uint8_t  base_middle;         // The next 8 bits of the base.
   uint8_t  access;              // Access flags, determine what ring this segment can be used in.
   uint8_t  granularity;         // Actually, also contains the bits 19:16 of segment limit
   uint8_t  base_high;           // The last 8 bits of the base.
} __attribute__((packed));

struct gdt_ptr_t {
   uint16_t limit;               // The upper 16 bits of all selector limits.
   uint32_t base;                // The address of the first gdt_entry_t struct.
} __attribute__((packed));

static gdt_entry_t gdt_entries[6];
static gdt_ptr_t   gdt_ptr;
static tss_entry_t tss;

void GDT::init() {
    gdt_ptr.limit = (sizeof(gdt_entry_t) * (sizeof(gdt_entries)/sizeof(*gdt_entries))) - 1;
    gdt_ptr.base = (uint32_t)&gdt_entries;

    set_descriptor(0, 0x0, 0x0, 0x0, 0x0);  /* NULL descriptor */
    set_descriptor(
        1, 
        0x0, 0xFFFFFFFF, 
        GDT_READABLE|GDT_CODE|GDT_TYPE(1)|GDT_PRESENT,
        GDT_32BIT|GDT_GRAN4K
    );  /* Kernel code */
    set_descriptor(
        2,
        0x0, 0xFFFFFFFF,
        GDT_WRITABLE|GDT_TYPE(1)|GDT_PRESENT,
        GDT_32BIT|GDT_GRAN4K
    ); /* Kernel data */
    set_descriptor(
        3,
        0x0, 0xFFFFFFFF,
        GDT_READABLE|GDT_CODE|GDT_TYPE(1)|GDT_DPL(3)|GDT_PRESENT,
        GDT_32BIT|GDT_GRAN4K
    ); /* User code */
    set_descriptor(
        4,
        0x0, 0xFFFFFFFF,
        GDT_WRITABLE|GDT_TYPE(1)|GDT_DPL(3)|GDT_PRESENT,
        GDT_32BIT|GDT_GRAN4K
    );  /* User data */

    bzero(&tss, sizeof(tss));
    tss.ss0 = KERNEL_DATA_SEG;
    tss.cs = KERNEL_CODE_SEG | 3;
    tss.ss = tss.es = tss.ds = tss.fs = tss.gs = KERNEL_DATA_SEG | 3;
    tss.iomap_base = ((uint32_t)&tss) + sizeof(tss);
    set_descriptor(
        5,
        (uint32_t)&tss,
        ((uint32_t)&tss) + sizeof(tss),
        GDT_DPL(3)|GDT_CODE|GDT_ACCESSED|GDT_PRESENT,
        0
    ); /* TSS */

    flush();
    tss_flush();

    /*
     * Set IOPL to ring0
     *  IOPL is stored in bis 12-13 of EFLAGS. Mask them out
     * */
    uint32_t eflags;
    read_eflags(eflags);
    TRACE("EFLAGS: 0x%X", eflags);

    eflags &= ~(3 << 12);
    assert( (eflags & 0x3000) == 0);
    write_eflags(eflags);
}

void GDT::set_descriptor(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;

    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access      = access;
}

void GDT::dump() {
    TRACE("GDT entries:");
    for(int i=0; i<sizeof(gdt_entries)/sizeof(*gdt_entries); i++) {
        TRACE("\taccess: 0x%X gran: 0x%X", gdt_entries[i].access, gdt_entries[i].granularity);
    }
}

extern "C" void gdt_flush(gdt_ptr_t*);
void GDT::flush() {
    gdt_flush(&gdt_ptr);
}

void GDT::tss_flush() {
    /* Set TSS to (TSS_SEG|0x3) */
    asm(
        ".intel_syntax noprefix\n"
        "mov ax, 0x2b\n"
        "ltr ax\n"
        ::: "ax"
    );
}

void GDT::set_kernel_stack(const void* stack) {
    tss.esp0 = (uint32_t) stack;
}


extern "C" 
void set_kernel_stack(const void* stack) {
    GDT::set_kernel_stack(stack);
}

