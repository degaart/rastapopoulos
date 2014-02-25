#include <stdint.h>
#include "kterm.h"
#include "kstub.h"
#include "kutil.h"
#include "kstring.h"
#include "gdt.h"

static struct TSS __attribute__ ((aligned (4096))) tss;
static struct GDT_ENTRY gdt[6];

static struct GDT_ENTRY encode_tss(uint32_t base, uint32_t limit, uint32_t flags) {
	struct GDT_ENTRY entry;
	entry.lo = MAKEDWORD(LOWORD(limit), LOWORD(base));
	entry.hi =
        ((base & 0xFF0000)>>16)|
        (flags & 0xD0FF00)|
        (((limit & 0xF0000)>>16)<<16)|
        (((base & 0xFF000000)>>24)<<24);
	return(entry);
}

static struct GDT_ENTRY encode_gdt(uint32_t base, uint32_t limit, uint32_t flags) {
	if(flags & GDT_GRAN(1)) {
		limit /= 4096;
	} else {
		if(limit & 0xFFF00000)
			PANIC("Invalid segment descriptor limit");
	}
    
	struct GDT_ENTRY entry;
	entry.lo = MAKEDWORD(LOWORD(limit), LOWORD(base));
	entry.hi =
        ((base & 0xFF0000)>>16)|
        (flags & 0xD0FF00)|
        (((limit & 0xF0000)>>16)<<16)|
        (((base & 0xFF000000)>>24)<<24);
    return(entry);
}

void gdt_init() {
	gdt[0] = encode_gdt(0, 0, 0);
	gdt[1] = encode_gdt(0, UINT32_MAX-1, GDT_CODE0);	/* Kernel code */
	gdt[2] = encode_gdt(0, UINT32_MAX-1, GDT_DATA0);	/* Kernel data */
	gdt[3] = encode_gdt(0, UINT32_MAX-1, GDT_CODE3);	/* User-mode code */
	gdt[4] = encode_gdt(0, UINT32_MAX-1, GDT_DATA3);	/* User-mode data */

	bzero(&tss, sizeof(tss));
	tss.ss0 = KERNEL_DATA_SEL;
	tss.esp0 = 0x7FFFF;
	tss.cs = KERNEL_CODE_SEL/*|0x3*/;
	tss.ss = tss.ds = tss.es = tss.fs = tss.gs = KERNEL_DATA_SEL/*|0x3*/;
	gdt[5] = encode_tss(
		(uint32_t)&tss,
		sizeof(tss)-1,
		TSS_TYPE(TSS_TYPE_AVAIL)|
		TSS_DPL(3)|
		TSS_GRANULARITY(0)|
		TSS_PRESENT(1)
	);

	_gdt_load(gdt, sizeof(gdt));
	_tss_load(0x28|0x3);
}


