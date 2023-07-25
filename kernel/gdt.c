#include "gdt.h"
#include "debug.h"
#include "util.h"
#include <stdint.h>
#include <string.h>

#define GDT_ACCESSED 1
#define GDT_WRITABLE (1 << 1)
#define GDT_DIRECTION (1 << 2)
#define GDT_TYPE(t) (((t)&1) << 4) // 0: system, 1: code/data
#define GDT_DPL(d) ((d & 0x3) << 5)
#define GDT_PRESENT (1 << 7)

#define GDT_READABLE (1 << 1)
#define GDT_CONFORMING (1 << 2)
#define GDT_CODE (1 << 3)
#define GDT_DEFAULT (1 << 6)

#define GDT_AVAIL(a) (((a)&1) << 4)
#define GDT_16BIT 0
#define GDT_32BIT (1 << 6)
#define GDT_GRAN1B 0
#define GDT_GRAN4K (1 << 7)

struct gdt_entry {
    uint16_t limit_low;  // The lower 16 bits of the limit.
    uint16_t base_low;   // The lower 16 bits of the base.
    uint8_t base_middle; // The next 8 bits of the base.
    uint8_t access; // Access flags, determine what ring this segment can be
                    // used in.
    uint8_t
        granularity; // Actually, also contains the bits 19:16 of segment limit
    uint8_t base_high; // The last 8 bits of the base.
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit; // The upper 16 bits of all selector limits.
    uint32_t base;  // The address of the first gdt_entry struct.
} __attribute__((packed));

struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
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
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap;
} __attribute__((packed));

static struct gdt_entry gdt_entries[6];
static struct gdt_ptr gdt_ptr;
static struct tss_entry tss;

void gdt_flush(void* gdtr);

void tss_set_esp0(const void* esp0)
{
    tss.esp0 = (uint32_t)esp0;
}

static inline void and_eflags(uint32_t mask)
{
    asm volatile("pushf\n"
                 "pop %%eax\n"
                 "and %%eax, %0\n"
                 "push %%eax\n"
                 "popf\n"
                 : "=r"(mask)
                 :
                 : "eax", "memory");
}

static void set_descriptor(int num, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t gran)
{
    gdt_entries[num].base_low = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;

    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access = access;
}

void gdt_init()
{
    gdt_ptr.limit = (sizeof(struct gdt_entry) *
                     (sizeof(gdt_entries) / sizeof(*gdt_entries))) -
                    1;
    gdt_ptr.base = (uint32_t)&gdt_entries;

    set_descriptor(0, 0x0, 0x0, 0x0, 0x0); /* NULL descriptor */
    set_descriptor(1, 0x0, 0xFFFFFFFF,
                   GDT_READABLE | GDT_CODE | GDT_TYPE(1) | GDT_PRESENT,
                   GDT_32BIT | GDT_GRAN4K); /* Kernel code */
    set_descriptor(2, 0x0, 0xFFFFFFFF, GDT_WRITABLE | GDT_TYPE(1) | GDT_PRESENT,
                   GDT_32BIT | GDT_GRAN4K); /* Kernel data */
    set_descriptor(3, 0x0, 0xFFFFFFFF,
                   GDT_READABLE | GDT_CODE | GDT_TYPE(1) | GDT_DPL(3) |
                       GDT_PRESENT,
                   GDT_32BIT | GDT_GRAN4K); /* User code */
    set_descriptor(4, 0x0, 0xFFFFFFFF,
                   GDT_WRITABLE | GDT_TYPE(1) | GDT_DPL(3) | GDT_PRESENT,
                   GDT_32BIT | GDT_GRAN4K); /* User data */
    set_descriptor(5, (uint32_t)&tss, (uint32_t)&tss + sizeof(tss),
                   GDT_CODE | GDT_PRESENT | GDT_ACCESSED, 0); /* TSS */

    gdt_flush(&gdt_ptr);

    /*
     * Set IOPL to ring0
     *  IOPL is stored in bis 12-13 of EFLAGS. Mask them out
     * */
    and_eflags(~(3 << 12));

    /*
     * Install tss
     */
    extern unsigned char _stacktop[];
    memset(&tss, 0, sizeof(tss));
    tss.ss0 = GDT_DS_KERNEL;
    tss.esp0 = (uint32_t)_stacktop;
    tss.cs = GDT_CS_KERNEL | 0x3;
    tss.ss = tss.ds = tss.es = tss.fs = tss.gs = GDT_DS_KERNEL | 0x3;
    tss.iomap = sizeof(tss);
    asm volatile("ltr ax" ::"a"(GDT_TSS) : "memory");
}
