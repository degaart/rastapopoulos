#include <stdint.h>
#include "process.h"
#include "kutil.h"
#include "kterm.h"
#include "kstring.h"
#include "kstub.h"
#include "kmalloc.h"
#include "vmm.h"
#include "pmm.h"
#include "hello.h"

#define PROCESS_ENTRY (128*1024*1024) /* 0x8000000 */

void process_create(struct PROCESS* proc, void* address) {
	bzero(proc, sizeof(struct PROCESS*));
	
	/*
		Create new pagedir for process
		In the name of simplicity, we just
		copy the current kernel pagedir, hoping
		for the best
	*/
	proc->pagedir = pmm_alloc_range(1024);
	uint32_t* page_dir = (uint32_t*)vmm_placement_alloc(proc->pagedir, 1024);
	TRACE("page_dir: %X", page_dir);
	vmm_copy_pagedir(page_dir);

	/*
		Now, we need to map memory for the process's code
		The process's executable code should start at 128MB
		For now, we only need one page
	*/
	uint32_t page_table_physical = pmm_alloc_range(1024);
	page_dir[32] = 
			PAGE_DIR_PRESENT|
			PAGE_DIR_RDWRITE|
			PAGE_DIR_PAGETABLE(page_table_physical)|
			PAGE_DIR_USER|
			PAGE_DIR_RDWRITE;
	vmm_placement_free(page_dir, 1024);
	
	/*
		Tricky, ain't it? we need to access page_table_physical
		So we need to vmm_map it
	*/
	uint32_t* page_table = (uint32_t*)vmm_placement_alloc(page_table_physical, 1024);
	bzero(page_table, 1024*4096);
	uint32_t process_memory_physical = pmm_alloc_page();
	page_table[0] = 
			PAGE_ENTRY_PRESENT|
			PAGE_ENTRY_BASE(process_memory_physical)|
			PAGE_ENTRY_RDWRITE|
			PAGE_ENTRY_USER;
	vmm_placement_free(page_table, 1024);
	
	/* Now, we need to copy process image into process_memory_physical */
	uint8_t* process_image = (uint8_t*)vmm_placement_alloc(process_memory_physical, 1);
	TRACE("process_image: %X", process_image);
	memcpy(process_image, hello_bin, hello_bin_size);
//	vmm_placement_free(process_image, 1);
	
	/*
		Wait! we also need to alloc space for the stack of the process, and map it!
		We're so lazy, we just put the stack at the end of the process's page
	*/
	proc->ss = USER_DATA_SEL|0x3;
	//proc->esp = process_memory_physical+4095;		// this didn't work (unmapped address)
	proc->esp = process_memory_physical+511;

	/* Now, what remains is to set eip */
	proc->eip = PROCESS_ENTRY;
}
