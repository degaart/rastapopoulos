#ifndef _GDT_H_
#define _GDT_H_

#define GDT_AVL(x)		((x)<<20)
#define GDT_SIZE(x)		((x)<<22)		/* 0: 16-bits, 1: 32-bits */
#define GDT_DPL(x)		((x)<<13)
#define GDT_GRAN(x)		((x)<<23)		/* 0: 1-bytes, 1: 4096 bytes */
#define GDT_PRESENT(x)	((x)<<15)
#define GDT_DESCTYPE(x)	((x)<<12)		/* 0: system, 1: code/data */
#define GDT_SEGTYPE(x)	((x)<<8)

#define GDT_SEG_TYPE(x)			((x)<<3)	/* 0: data, 1: code */
#define GDT_SEG_ACCESSED(x)		(x)

#define GDT_DSEG_EDOWN(x)		((x)<<2)	/* 0: expand-up, 1: expand-down */
#define GDT_DSEG_WRITABLE(x)	((x)<<1)	/* 0: read-only, 1: read-write */

#define GDT_CSEG_CONFORMING(x)	((x)<<2)	/* 0: non-conforming, 1: conforming */
#define GDT_CSEG_READ(x)		((x)<<1)	/* 0: execute-only, 1: execute-read */

#define GDT_SSEG_TSS16			(1)
#define GDT_SSEG_LDT			(2)
#define GDT_SSEG_TSS16_BUSY		(3)
#define GDT_SSEG_CGATE16		(4)
#define GDT_SSEG_TGATE			(5)
#define GDT_SSEG_IGATE16		(6)
#define GDT_SSEG_TRGRATE16		(7)		/* 16-bit trap gate */
#define GDT_SSEG_TSS32			(9)		/* 32-bit tss */
#define GDT_SSEG_TSS32_BUSY		(11)	/* 32-bit tss (busy */
#define GDT_SSEG_CGATE32		(23)	/* 32-bit call gate */
#define GDT_SSEG_IGATE32		(14)	/* 32-bit interrupt gate */
#define GDT_SSEG_TRGATE32		(15)	/* 32-bit trap gate */

#define GDT_CODE0				(GDT_SIZE(1)| \
								GDT_DPL(0)|	\
								GDT_GRAN(1)| \
								GDT_PRESENT(1)| \
								GDT_DESCTYPE(1)| \
								GDT_SEGTYPE( \
									GDT_SEG_TYPE(1)|	\
									GDT_CSEG_CONFORMING(0)|	\
									GDT_CSEG_READ(1) \
								))
#define GDT_DATA0				(GDT_SIZE(1)| \
								GDT_DPL(0)|	\
								GDT_GRAN(1)| \
								GDT_PRESENT(1)| \
								GDT_DESCTYPE(1)| \
								GDT_SEGTYPE( \
									GDT_SEG_TYPE(0)|	\
									GDT_DSEG_EDOWN(0)|	\
									GDT_DSEG_WRITABLE(1) \
								))

#define GDT_CODE3				(GDT_SIZE(1)| \
								GDT_DPL(3)|	\
								GDT_GRAN(1)| \
								GDT_PRESENT(1)| \
								GDT_DESCTYPE(1)| \
								GDT_SEGTYPE( \
									GDT_SEG_TYPE(1)|	\
									GDT_CSEG_CONFORMING(0)|	\
									GDT_CSEG_READ(1) \
								))
#define GDT_DATA3				(GDT_SIZE(1)| \
								GDT_DPL(3)|	\
								GDT_GRAN(1)| \
								GDT_PRESENT(1)| \
								GDT_DESCTYPE(1)| \
								GDT_SEGTYPE( \
									GDT_SEG_TYPE(0)|	\
									GDT_DSEG_EDOWN(0)|	\
									GDT_DSEG_WRITABLE(1) \
								))



struct GDT_ENTRY {
	uint32_t lo;
	uint32_t hi;
} __attribute__((packed)); 

#define TSS_AVAIL(x)		((x)<<20)
#define TSS_TYPE(x)			((x)<<8)
#define TSS_TYPE_BUSY		0xB
#define TSS_TYPE_AVAIL		0x9
#define TSS_DPL(x)			((x)<<13)
#define TSS_GRANULARITY(x) 	((x)<<23)
#define TSS_PRESENT(x)		((x)<<15)

/*
	Should not cross a page boudary
*/
struct TSS {
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

void gdt_init();

#endif //_GDT_H_

