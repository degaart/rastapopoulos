#ifndef _VMM_H_
#define _VMM_H_

#include "idt.h"
#include "pagedir.h"
#include <stdint.h>

/*
	Virtual memory manager

	Assumes all pagedirs are stored in kernel-space
*/
class VMM {
private:
	static void page_fault_handler(const isr_regs_t* regs);
	static void double_fault_handler(const isr_regs_t* regs);

	static bool _paging_enabled;

	static Pagedir* _current_pagedir;
public:
	static const uint32_t PAGE_SIZE = 0x1000;
	static const uint8_t* INITIAL_KERNEL_STACK;

	static void init();

	/* Is paging enabled yet? */
	static bool paging_enabled();

	/* Create new Pagedir */
	static Pagedir* create_pagedir();

	/* destroy Pagedir */
	static void free_pagedir(Pagedir* pagedir);

	/* set current page directory */
	static void switch_pagedir(Pagedir*);

	/*
		Map physical address into virtual address
		PAGE_PRESENT is not implied in flags, so need to supply it when calling this function

		If address is already mapped, and MAP_REMAP is not given, panics
	*/
	static const uint32_t PAGE_PRESENT	= Pagedir::PTE_PRESENT;
	static const uint32_t PAGE_WRITABLE	= Pagedir::PTE_WRITABLE;
	static const uint32_t PAGE_USER		= Pagedir::PTE_USER;
	static void map(uint32_t va, uint32_t pa, uint32_t flags);
	static void map(void* va, uint32_t pa, uint32_t flags) {
		map((uint32_t)va, pa, flags);
	}

	/* Unmap virtual address */
	static void unmap(uint32_t va);
	static void unmap(void* va) {
		unmap((uint32_t)va);
	}

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
