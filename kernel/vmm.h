#ifndef _VMM_H_
#define _VMM_H_

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
	#define PAGE_DIR_BASE(x)		( ( (uint32_t) (x) ) & 0xFFFFF000 )
	#define PAGE_DIR_DATA2(x)		((x) >> 1)				/* Only available if PAGE_DIR_PRESENT not set */
	#define PAGE_DIR_PAGETABLE(x)	( ( (uint32_t) (x) ) & 0xFFFFF000 )
	#define PAGE_DIR_PAGETABLE_MASK 0xFFFFF000
	
	
	
	#define PAGE_ENTRY_PRESENT		PAGE_DIR_PRESENT
	#define PAGE_ENTRY_RDONLY		PAGE_DIR_RDONLY
	#define PAGE_ENTRY_RDWRITE		PAGE_DIR_RDWRITE
	#define PAGE_ENTRY_USER			PAGE_DIR_USER
	#define PAGE_ENTRY_SUPERVISOR	PAGE_DIR_SUPERVISOR
	#define PAGE_ENTRY_WRTHROUGH	PAGE_DIR_WRTHROUGH
	#define PAGE_ENTRY_UNCACHED		PAGE_DIR_UNCACHED
	#define PAGE_ENTRY_ACCESSED		PAGE_DIR_ACCESSED
	#define PAGE_ENTRY_DIRTY		(0x1<<6)
	#define PAGE_ENTRY_GLOBAL		PAGE_DIR_GLOBAL
	#define PAGE_ENTRY_DATA(x)		PAGE_DIR_DATA(x)
	#define PAGE_ENTRY_BASE(x)		( ( (uint32_t) (x) ) & 0xFFFFF000 )
	#define PAGE_ENTRY_BASE_MASK	0xFFFFF000
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

	void vmm_init();
	void vmm_map(uint32_t linear_address, uint32_t physical_address, uint32_t flags);
	void vmm_flush();
	void vmm_dump_mem_regions();
	int vmm_paging_enabled();

#endif //_VMM_H_