#include "gdt.h"
#include "string.h"
#include "debug.h"
#include "registers.h"
#include "pmm.h"
#include "kernel.h"
#include "locks.h"

#include <stdint.h>

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

struct tss_entry {
    uint16_t prev_tss;
    uint16_t reserved0;
    uint32_t esp0;
    uint16_t ss0;
    uint16_t reserved1;
    uint32_t esp1;
    uint16_t ss1;
    uint16_t reserved2;
    uint32_t esp2;
    uint16_t ss2;
    uint16_t reserved3;
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
    uint16_t es;
    uint16_t reserved4;
    uint16_t cs;
    uint16_t reserved5;
    uint16_t ss;
    uint16_t reserved6;
    uint16_t ds;
    uint16_t reserved7;
    uint16_t fs;
    uint16_t reserved8;
    uint16_t gs;
    uint16_t reserved9;
    uint16_t ldt;
    uint16_t reserved10;
    uint16_t trap;
    uint16_t iomap_base;
    uint8_t iomap[IOMAP_SIZE];
} __attribute__((packed));

struct gdt_entry {
   uint16_t limit_low;          // The lower 16 bits of the limit.
   uint16_t base_low;           // The lower 16 bits of the base.
   uint8_t  base_middle;        // The next 8 bits of the base.
   uint8_t  access;             // Access flags, determine what ring this segment can be used in.
   uint8_t  granularity;        // Actually, also contains the bits 19:16 of segment limit
   uint8_t  base_high;          // The last 8 bits of the base.
} __attribute__((packed));

struct gdt_ptr {
   uint16_t limit;              // The upper 16 bits of all selector limits.
   uint32_t base;               // The address of the first gdt_entry struct.
} __attribute__((packed));

static struct gdt_entry gdt_entries[6];
static struct gdt_ptr   gdt_ptr;
static struct tss_entry tss;

static void set_descriptor(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);

void gdt_init()
{
    gdt_ptr.limit = (sizeof(struct gdt_entry) * (sizeof(gdt_entries)/sizeof(*gdt_entries))) - 1;
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

    /*
     * I/O permissions checking
     * If CPL>IOPL, the processor checks iomap in the current TSS
     * If a bit is set in the iomap, the corresponding port causes a GPF on access in ring3
     * iomap must be terminated by a 0xFF
     */
    bzero(&tss, sizeof(tss));
    tss.ss0 = KERNEL_DATA_SEG;
    tss.esp0 = (uint32_t)(initial_kernel_stack + PAGE_SIZE);
    tss.cs = KERNEL_CODE_SEG | 3;
    tss.ss = tss.es = tss.ds = tss.fs = tss.gs = KERNEL_DATA_SEG | 3;
    tss.iomap_base = tss.iomap - (uint8_t*)&tss;
    memset(tss.iomap, 0xFF, sizeof(tss.iomap));
    set_descriptor(
        5,
        (uint32_t)&tss,
        sizeof(tss),
        GDT_DPL(3)|GDT_CODE|GDT_ACCESSED|GDT_PRESENT,
        0
    ); /* TSS */

    assert(sizeof(tss) >= 103);

    gdt_flush(&gdt_ptr);
    tss_flush();

    /*
     * Set IOPL to ring0
     *  IOPL is stored in bis 12-13 of EFLAGS. Mask them out
     * */
    uint32_t eflags = read_eflags();

    eflags &= ~(3 << 12);
    assert( (eflags & 0x3000) == 0);

    write_eflags(eflags);
}

static void set_descriptor(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;

    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access      = access;
}

void gdt_iomap_set(void* buffer, size_t size)
{
    assert(size <= sizeof(tss.iomap));
    memcpy(tss.iomap, buffer, size);
}

void tss_set_kernel_stack(void* esp0)
{
    enter_critical_section();
    tss.esp0 = (uint32_t)esp0;
    leave_critical_section();
}

void* tss_get_kernel_stack()
{
    enter_critical_section();
    void* result = (void*)tss.esp;
    leave_critical_section();
    return result;
}

