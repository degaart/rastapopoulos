#ifndef _VMM_H_
#define _VMM_H_

#include "idt.h"
#include <stdint.h>

/*
	Virtual memory manager

	Assumes all pagedirs are stored in kernel-space
*/
class VMM {
private:
	static void map_seg(uint32_t va, uint32_t pa, uint32_t flags, uint32_t options);
	static void page_fault_handler(const isr_regs_t* regs);
	static void double_fault_handler(const isr_regs_t* regs);

	static bool _paging_enabled;

	static const int PTE_PRESENT        =     1;
	static const int PTE_WRITABLE       =     2;
	static const int PTE_USER           =     4;
	static const int PTE_WRITETHOUGH    =     8;
	static const int PTE_NOT_CACHEABLE  =     0x10;
	static const int PTE_ACCESSED       =     0x20;
	static const int PTE_DIRTY          =     0x40;
	static const int PTE_PAT            =     0x80;
	static const int PTE_CPU_GLOBAL     =     0x10;
	static const int PTE_LV4_GLOBAL     =     0x200;
	static const int PTE_FRAME          =     0xFFFFF000;
	static const int PTE_OFFSET			= 	  0x00000FFF;


	static const int PDE_PRESENT        =     1;
	static const int PDE_WRITABLE       =     2;
	static const int PDE_USER           =     4;
	static const int PDE_PWT            =     8;
	static const int PDE_PCD            =     0x10;
	static const int PDE_ACCESSED       =     0x20;
	static const int PDE_DIRTY          =     0x40;
	static const int PDE_4MB            =     0x80;
	static const int PDE_CPU_GLOBAL     =     0x100;
	static const int PDE_LV4_GLOBAL     =     0x200;
	static const int PDE_FRAME          =     0xFFFFF000;

	struct pagetable_t {
	    uint32_t entries[1024];
	};

	struct pagedir_t {
	    uint32_t entries[1024];				/* Entries of pagedir, with flags etc, for dumping into cr3 */
	    pagetable_t* tables[1024];			/* Pagetables mapped in kernel-space for manipulation */
	    uint32_t physical;					/* Physical address of this pagedir */
	};

	static pagedir_t* _current_pagedir;
public:
	static const uint32_t PAGE_SIZE = 0x1000;
	static const uint8_t* INITIAL_KERNEL_STACK;

	static void init();

	/* Is paging enabled yet? */
	static bool paging_enabled();

	/* set current page directory */
	// static void set_pagedir(pagedir_t*);

	/*
		Map physical address into virtual address
		PAGE_PRESENT is not implied in flags, so need to supply it when calling this function

		If address is already mapped, and MAP_REMAP is not given, panics
	*/
	static const uint32_t PAGE_PRESENT	= PTE_PRESENT;
	static const uint32_t PAGE_WRITABLE	= PTE_WRITABLE;
	static const uint32_t PAGE_USER		= PTE_USER;

	static const uint32_t MAP_REMAP	= 0x1;										/* Allow remapping of the page if it already mapped */
	static void map(void* va, uint32_t pa, uint32_t flags, uint32_t options = 0);

	/* Unmap virtual address */
	static void unmap(uint32_t va);

	/* 
		Get physical address for a virtual address
		Returns false if the address is unmapped
	*/
	static bool get_physical(void* va, uint32_t* pa);

	/*
		Check if given VA is mapped
	*/
	static bool is_mapped(void* va);

	/*
		Flush TLB cache
	*/
	static void flush_tlb(void* va);	
};

#endif //_VMM_H_
