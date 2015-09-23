#ifndef _VMM_H_
#define _VMM_H_

#include "idt.h"
#include <stdint.h>

/*
	Virtual memory manager
*/
class VMM {
private:
	static void map_seg(uint32_t va, uint32_t pa, uint32_t flags);
	static void page_fault_handler(const isr_regs_t* regs);
	static void double_fault_handler(const isr_regs_t* regs);

	static bool _paging_enabled;
public:
	static const uint32_t PAGE_SIZE = 0x1000;

	static void init();

	/* Is paging enabled yet? */
	static bool paging_enabled();

	/* set current page directory */
	// static void set_pagedir(pagedir_t*);

	/* Map physical address into virtual address */
	static const uint32_t PAGE_PRESENT	= 0x1;
	static const uint32_t PAGE_WRITE	= 0x2;
	static const uint32_t PAGE_USER		= 0x4;
	static void map(uint32_t va, uint32_t pa, uint32_t flags);

	/* Unmap virtual address */
	static void unmap(uint32_t va);

	/* 
		Get physical address for a virtual address
		Returns false if the address is unmapped
	*/
	static bool get_physical(uint32_t va, uint32_t* pa);
};

#endif //_VMM_H_
