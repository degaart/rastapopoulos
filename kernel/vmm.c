#include <stdint.h>
#include "kutil.h"
#include "kterm.h"
#include "kmalloc.h"
#include "kstring.h"
#include "pmm.h"
#include "vmm.h"

#define PAGE_SIZE				4096		/* Size on an entry in the page table */
#define PAGE_DIRECTORY_SIZE 	(1024*PAGE_SIZE)	/* Size of an entry in the page directory */
#define VGA_PAGE				0xB8000

#define TABLE_SIZE          1024                        /* Amount of pages in a table */
#define DIR(page)           ((page)/TABLE_SIZE)           /* Directory entry for a given page */
#define ENTRY(page)         ((page)%TABLE_SIZE)           /* Entry in the directory for a given page */
#define TABLE(dir)			((uint32_t*)((dir) & PAGE_DIR_PAGETABLE_MASK)) /* Get pagetable from page directory */

#define INVALID_ADDRESS		((void*)UINT32_MAX)

static uint32_t* page_directory;			/* Page directory entries aren't pointers, beware */
static uint32_t paging_enabled = 0;

/*
	Initialize the vmmp by mapping currently used kernel memory
*/
void vmm_init() {
	/* First we initialize the physical memory manager */
	pmm_init();
	pmm_dump_mem_regions();
	
	/* Need to add manually memory which is not marked by the bios as reserved or free */
	pmm_add_region(VGA_PAGE, PAGE_SIZE, REGION_RESERVED);

	/* Allocate the page directory */
	page_directory = (uint32_t*)kmalloc_seg_a(1024*sizeof(uint32_t), PAGE_SIZE);
	bzero(page_directory, 1024*sizeof(uint32_t));
	
	/*
		Now, need to setup identity paging for the currently used memory
		Don't fucking forget: it needs to be fucking writable
	*/
	for(
		uint32_t location=0;
		location <= ALIGN32((uint32_t)kmalloc_seg_get_start(), PAGE_SIZE); /* This may change after a call to vmm_map */
		location += 4096
	) {
		uint32_t page_status = pmm_page_status(location);
		if((page_status == PMM_STATUS_FREE)||(page_status == PMM_STATUS_RESERVED)) {
			vmm_map(location, location, PAGE_ENTRY_SUPERVISOR|PAGE_ENTRY_RDWRITE);
			if(page_status == PMM_STATUS_FREE)
				pmm_reserve(location);
		}
	}
	
	/* That's all, folks */
	vmm_flush();
	_write_cr0(_read_cr0() | CR0_PAGING);
	paging_enabled = 1;
	
	/* Print information about usable memory */
	write_format(
		"Kernel starts at: %X\n"
		"Kernel ends at: %X\n",
		0x100000,
		kmalloc_seg_get_start()
	);
	write_format("Kernel memory: %u Kb\n", (((uint32_t)kmalloc_seg_get_start())-0x100000)/1024);
	write_format("Free memory: %u Kb\n", pmm_get_free()/1024);
}

/*
	Map a linear address to a physical address
	This function does not tell to the physical memory
	address that a location has been mapped, fucking deal
	with it elsewhere
*/
void vmm_map(uint32_t linear_address, uint32_t physical_address, uint32_t flags) {
	/*
		Check with PMM if this page is usable
		Note that we can map reserved addresses
		We can also map physical adresses which are
		already allocated (map physical addresses
		to multiple linear addresses, for kernel for example)
		
		This leaves an absent physical address as only error
		condition
	*/
	switch(pmm_page_status(physical_address)) {
	case PMM_STATUS_ABSENT:
		PANIC("Trying to map an absent page at physical address %X", physical_address);
		break;
	}

	/* Get directory entry for this linear address */
	ASSERT( (linear_address % PAGE_SIZE) == 0 );
	ASSERT( (physical_address % PAGE_SIZE) == 0 );
	
	uint32_t page = linear_address/PAGE_SIZE;
	uint32_t directory_entry = DIR(page);
	uint32_t pagetable_entry = ENTRY(page);

	/*
		If no pagetable for current page directory
		need to allocate memory for it
	*/
	if(!(page_directory[directory_entry] & PAGE_DIR_PRESENT)) {
		uint32_t* page_table = (uint32_t*)kmalloc_seg_a(1024*sizeof(uint32_t), 4); /* Page directories need not be aligned to PAGE_SIZE */
		bzero(page_table, 1024*sizeof(uint32_t));
		
		page_directory[directory_entry] = 
					PAGE_DIR_PRESENT|
					PAGE_DIR_RDWRITE|
					PAGE_DIR_SIZE4K|
					PAGE_DIR_PAGETABLE(page_table)|
					PAGE_DIR_USER|
					PAGE_DIR_RDWRITE|
					flags;
	}
	
	uint32_t* page_table = TABLE(page_directory[directory_entry]);
	if(page_table[pagetable_entry] & PAGE_ENTRY_PRESENT)
		PANIC("Linear address already mapped: %X", linear_address);

	page_table[pagetable_entry] = 
			PAGE_ENTRY_PRESENT|
			PAGE_ENTRY_BASE(physical_address)|
			flags;
}

void vmm_unmap(uint32_t linear_address) {
	if(linear_address % 4096)
		PANIC("Invalid linear address: %X", linear_address);

	uint32_t page = linear_address/PAGE_SIZE;
	uint32_t directory_entry = DIR(page);
	uint32_t pagetable_entry = ENTRY(page);
	if(page_directory[directory_entry] & PAGE_DIR_PRESENT) {
		uint32_t* page_table = (uint32_t*)(page_directory[directory_entry] & PAGE_DIR_PAGETABLE_MASK);
		if(page_table[pagetable_entry] & PAGE_ENTRY_PRESENT) {
			page_table[pagetable_entry] &= ~PAGE_ENTRY_PRESENT;
			return;
		}
	}
	PANIC("Unmapped linear address: %X", linear_address);
}

void vmm_flush() {
	_write_cr3((uint32_t)page_directory);
}

void vmm_dump_mem_regions() {
	pmm_dump_mem_regions();
}

int vmm_paging_enabled() {
	return(paging_enabled);
}

/*
	My! this is ugly
*/
static int vmm_linear_contiguous(uint32_t page, uint32_t count) {
	unsigned dir = DIR(page);
	unsigned entry = ENTRY(page);
	while((dir<1024) && (entry<1024) && count) {
		if(entry == 1024) {
			entry = 0;
			dir++;
		}
		if(page_directory[dir] & PAGE_DIR_PRESENT) {
			if(TABLE(page_directory[dir])[entry] & PAGE_DIR_PRESENT)
				return(0);
		}
		entry++;
		count--;
	}
	return(1);
}

static void* vmm_find_free_linear(uint32_t bytecount) {
	bytecount = ALIGN32(bytecount, PAGE_SIZE);
	int pagecount = bytecount / PAGE_SIZE;
	
	for(uint32_t page=0; page<UINT32_MAX/PAGE_SIZE; page++) {
		if(vmm_linear_contiguous(page, pagecount)) {
			return((void*)(page*PAGE_SIZE));
		} else {
			page += pagecount;
		}
	}
	return((void*)UINT32_MAX);
}

/* Allocate a range of pages, and return the resulting linear address */
void* vmm_alloc_pages(uint32_t pages_count) {
	/* We don't need an interrupt messing with pmm */
	pushf();
	cli();
	
	void* physical_address = (void*)pmm_alloc_range(pages_count);
	if(physical_address == INVALID_ADDRESS)
		PANIC("Physical memory exhaustion");
		
	void* linear_address = vmm_find_free_linear(pages_count*PAGE_SIZE);
	if(linear_address == INVALID_ADDRESS)
		PANIC("Linear address exhaustion");
		
	vmm_map(
		(uint32_t)linear_address,
		(uint32_t)physical_address,
		PAGE_ENTRY_RDWRITE|
		PAGE_ENTRY_SUPERVISOR
	);
	
	/* Flush our tables */
	vmm_flush();

	/* Reenable interrupts */	
	popf();
	return(linear_address);
}

