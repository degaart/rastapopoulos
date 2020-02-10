#include "gdt.h"
#include "string.h"
#include "debug.h"
#include "registers.h"

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

#define IOMAP_SIZE ((65536 / 8) + 1)

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
   struct gdt_entry* base;               // The address of the first gdt_entry struct.
} __attribute__((packed));

static struct gdt_entry gdt[6];
static struct tss_entry tss;
static struct gdt_ptr gdt_ptr;
extern unsigned char stack_bottom[];
extern unsigned char stack_top[];

void gdt_flush(struct gdt_ptr*);

static
struct gdt_entry gdt_make_descriptor(uint32_t base, uint32_t limit, uint8_t access, uint16_t gran)
{
    struct gdt_entry result;

    result.base_low     = (base & 0xFFFF);
    result.base_middle  = (base >> 16) & 0xFF;
    result.base_high    = (base >> 24) & 0xFF;
    result.limit_low    = (limit & 0xFFFF);
    result.granularity  = (limit >> 16) & 0x0F;
    result.granularity |= gran & 0xF0;
    result.access       = access;

    return result;
}

void gdt_init()
{
    gdt[0] = gdt_make_descriptor(0, 0, 0, 0);          /* NULL descriptor */
    gdt[1] = gdt_make_descriptor(0, 0xffffffff, GDT_READABLE|GDT_CODE|GDT_TYPE(1)|GDT_PRESENT, GDT_32BIT|GDT_GRAN4K);      /* Kernel code */
    gdt[2] = gdt_make_descriptor(0, 0xffffffff, GDT_WRITABLE|GDT_TYPE(1)|GDT_PRESENT, GDT_32BIT|GDT_GRAN4K);      /* Kernel data */
    gdt[3] = gdt_make_descriptor(0, 0xffffffff, GDT_READABLE|GDT_CODE|GDT_TYPE(1)|GDT_DPL(3)|GDT_PRESENT, GDT_32BIT|GDT_GRAN4K);      /* User code */
    gdt[4] = gdt_make_descriptor(0, 0xffffffff, GDT_WRITABLE|GDT_TYPE(1)|GDT_DPL(3)|GDT_PRESENT, GDT_32BIT|GDT_GRAN4K);      /* User data */

    bzero(&tss, sizeof(tss));
    tss.ss0 = KERNEL_DATA_SEG;
    tss.esp0 = ((unsigned long)&stack_top - sizeof(uint32_t)) & 0xFFFFFFF0;
    tss.cs = KERNEL_CODE_SEG | 3;
    tss.ss = tss.es = tss.ds = tss.fs = tss.gs = KERNEL_DATA_SEG | 3;
    tss.iomap_base = tss.iomap - (unsigned char*)&tss;

    bzero(tss.iomap, sizeof(tss.iomap)); /* TODO: Remove this */
    tss.iomap[IOMAP_SIZE - 1] = 0xFF;
    
    //memset(tss.iomap, 0xFF, sizeof(tss.iomap));
    gdt[5] = gdt_make_descriptor((uint32_t)&tss, sizeof(tss), GDT_DPL(3)|GDT_CODE|GDT_ACCESSED|GDT_PRESENT, 0);

    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base = gdt;
    gdt_flush(&gdt_ptr);
    tss_flush();

    /* set iopl to ring0 */
    unsigned long eflags = read_eflags();
    eflags &= ~(3 << 12);
    write_eflags(eflags);
}


