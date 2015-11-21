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
	static void page_fault_handler(isr_regs_t* regs);
	static void double_fault_handler(isr_regs_t* regs);

	static bool _paging_enabled;

	static Pagedir* _current_pagedir;
public:
	static const uint32_t PAGE_SIZE = 0x1000;

	static const uint32_t INITIAL_KERNEL_STACK = 0x7BFF;
	static const uint32_t USERSPACE_START = 0x400000;
	static const uint32_t USERSPACE_END = 0xBFFFFFFF;

	static void init();

	/* Is paging enabled yet? */
	static bool paging_enabled();

	/* Create new Pagedir */
	static Pagedir* create_pagedir();
	
	/* destroy Pagedir */
	static void free_pagedir(Pagedir* pagedir);

	/* set current page directory */
	static void set_pagedir(Pagedir*);

	/* get current page directory */
	static Pagedir* current_pagedir();

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
		Allocates page frame and map to specified virtual address
		Physical addresses not guaranteed to be contiguous	
	*/
	static bool alloc(uint32_t va, uint32_t size, uint32_t flags);
	static bool alloc(uint32_t va , uint32_t flags) {
		return alloc(va, VMM::PAGE_SIZE, flags);
	}

	static bool alloc(void* va, uint32_t size, uint32_t flags) {
		return alloc((uint32_t)va, size, flags);
	}

	static bool alloc(void* va, uint32_t flags) {
		return alloc(va, VMM::PAGE_SIZE, flags);
	}

	/*
		Unmaps specified virtual address and frees page-frame as well
	*/
	static void dealloc(uint32_t va, uint32_t size = VMM::PAGE_SIZE);
	static void dealloc(void* va, uint32_t size = VMM::PAGE_SIZE) {
		dealloc((uint32_t)va, size);
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

	/*
		Clone current pagedir
		(Note: only clones user-pages, kernel pages are left unmapped)
	*/
	static Pagedir* clone_pagedir();
};

#endif //_VMM_H_
