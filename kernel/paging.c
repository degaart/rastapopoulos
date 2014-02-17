#include <stdint.h>
#include "kutil.h"
#include "paging.h"
#include "kmalloc.h"
#include "kstring.h"

#define PAGE_DIR_PRESENT		0x1
#define PAGE_DIR_RDONLY			0x0
#define PAGE_DIR_RDWRITE		(0x1<<1)
#define PAGE_DIR_USER			(0x1<<2)
#define PAGE_DIR_SUPERVISOR		0x0
#define PAGE_DIR_WRTHROUGH		(0x1<<3)
#define PAGE_DIR_UNCACHED		(0x1<<4)
#define PAGE_DIR_ACCESSED		(0x1<<5)
#define PAGE_DIR_SIZE4K			0x0
#define PAGE_DIR_SIZE4M			(0x1<<7)
#define PAGE_DIR_GLOBAL			(0x1<<8)
#define PAGE_DIR_DATA(x)		(((x) & 0x7) << 9)
#define PAGE_DIR_BASE(x)		( ( (uint32_t) x ) & 0xFFFFF000 )
#define PAGE_DIR_DATA2(x)		((x) >> 1)				/* Only available if PAGE_DIR_PRESENT not set */

#define PAGE_ENTRY_PRESENT		PAGE_DIR_PRESENT
#define PAGE_ENTRY_RDONLY		PAGE_DIR_RDONLY
#define PAGE_ENTRY_USER			PAGE_DIR_USER
#define PAGE_ENTRY_SUPERVISOR	PAGE_DIR_SUPERVISOR
#define PAGE_ENTRY_WRTHROUGH	PAGE_DIR_WRTHROUGH
#define PAGE_ENTRY_UNCACHED		PAGE_DIR_UNCACHED
#define PAGE_ENTRY_ACCESSED		PAGE_DIR_ACCESSED
#define PAGE_ENTRY_DIRTY		(0x1<<6)
#define PAGE_ENTRY_GLOBAL		PAGE_DIR_GLOBAL
#define PAGE_ENTRY_DATA(x)		PAGE_DIR_DATA(x)
#define PAGE_ENTRY_BASE(x)		PAGE_DIR_BASE(x)
#define PAGE_ENTRY_DATA2(x)		PAGE_DIR_DATA2(x)

#define CR0_PAGING				(1<<31)
#define CR0_CACHE_DISABLE		(1<<30)
#define CR0_NOT_WRTHROUGH		(1<<29)
#define CR0_ALIGN_CHECK			(1<<18)
#define CR0_WRIPTE_PROTECT		(1<<16)
#define CR0_NUMERIC_ERROR		(1<<5)
#define CR0_EXTENSION_TYPE		(1<<4)
#define CR0_FP_TASK_SWITCHED	(1<<3)
#define CR0_FP_EMULATION		(1<<2)
#define CR0_FP_MONITOR			(1<<1)
#define CR0_PROTECTION			(1)

#define CR4_V8086				(1)
#define CR4_PMVIF				(1<<1)
#define CR4_TS_DISABLE			(1<<2) /* Restricts RDTSC */
#define CR4_DEBUG_EXTENSIONS	(1<<3)
#define CR4_PSE					(1<<4)
#define CR4_PAE					(1<<5)
#define CR4_MCE					(1<<6)
#define CR4_PAGE_GLOBAL_ENABLE	(1<<7)
#define CR4_PMC_ENABLE			(1<<8)

void paging_init() {
	uint32_t* page_table = (uint32_t*)kmalloc_seg_a(1024*sizeof(uint32_t), 4096);

	/* Map first 4Mb for kernel */
	uint32_t page_start = 0;
	for(int i=0; i<1024; i++) {
		page_table[i] = 
			PAGE_ENTRY_PRESENT|
			PAGE_ENTRY_SUPERVISOR|
			PAGE_DIR_BASE(page_start);
		page_start += 4096;
		/*DUMP32(page_table[i]);
		
		if(i==16)
			_halt();*/
	}
	
	uint32_t* page_directory = (uint32_t*)kmalloc_seg(1024*sizeof(uint32_t), 4096);
	bzero(page_directory, 1024*sizeof(uint32_t));
	page_directory[0] = 
		PAGE_DIR_PRESENT|
		PAGE_DIR_RDWRITE|
		PAGE_DIR_SUPERVISOR|
		PAGE_DIR_SIZE4K|
		PAGE_DIR_BASE(page_table);
	_write_cr3((uint32_t)page_directory);
	_write_cr0(_read_cr0() | CR0_PAGING);
	//_halt();
}
