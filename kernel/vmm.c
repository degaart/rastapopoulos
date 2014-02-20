#include <stdint.h>
#include "kutil.h"
#include "kterm.h"
#include "kmalloc.h"
#include "kstring.h"
#include "pmm.h"
#include "vmm.h"

#define PAGE_SIZE			4096		/* Size on an entry in the page table */
#define PAGE_DIRECTORY_SIZE (1024*PAGE_SIZE)	/* Size of an entry in the page directory */
#define VGA_PAGE 0xB8000

static uint32_t* page_directory;
static uint32_t paging_enabled = 0;

/*
	Initialize the vmmp by mapping currently used kernel memory
*/
void vmm_init() {
	/* First we initialize the physical memory manager */
	pmm_init();
	pmm_dump_mem_regions();
	
	/* Need to add manually memory which is not marked by the bios as reserved or free */
	pmm_add_region(VGA_PAGE, 4096, REGION_RESERVED);

	/* Allocate the page directory */
	page_directory = (uint32_t*)kmalloc_seg_a(1024*sizeof(uint32_t), 4096);
	bzero(page_directory, 1024*sizeof(uint32_t));
	
	/*
		Now, need to setup identity paging for the currently used memory
		Don't fucking forget: it needs to be fucking writable
	*/
	for(
		uint32_t location=0;
		location <= (uint32_t)kmalloc_seg_get_start(); /* This may change after a call to vmm_map */
		location += 4096
	) {
		if(pmm_page_usable(location)) {
			vmm_map(location, location, PAGE_ENTRY_SUPERVISOR|PAGE_ENTRY_RDWRITE);
		}
	}

	/* Actually, we also need to map VGA memory at this point */
	vmm_map(VGA_PAGE, VGA_PAGE, PAGE_ENTRY_SUPERVISOR|PAGE_ENTRY_RDWRITE);
	
	/* That's all, folks */
	vmm_flush();
	_write_cr0(_read_cr0() | CR0_PAGING);
	paging_enabled = 1;
}

/*
	Map a linear address to a physical address
*/
void vmm_map(uint32_t linear_address, uint32_t physical_address, uint32_t flags) {
	/* Check with PMM if this page is usable */
	if(!pmm_page_usable(physical_address))
		PANIC("Trying to map an unusable page at physical address %X", physical_address);

	/* K it's usable, tell PMM about it */
	if(physical_address!=VGA_PAGE)
		pmm_reserve(linear_address);
	
	/* Get directory entry for this linear address */
	ASSERT( (linear_address % 4096) == 0 );
	ASSERT( (physical_address % 4096) == 0 );

	uint32_t directory_entry = linear_address / PAGE_DIRECTORY_SIZE;
	uint32_t pagetable_entry = (linear_address - (directory_entry*PAGE_DIRECTORY_SIZE))/PAGE_SIZE;

	/*
		If no pagetable for current page directory
		need to allocate memory for it
	*/
	if(!(page_directory[directory_entry] & PAGE_DIR_PRESENT)) {
		uint32_t* page_table = (uint32_t*)kmalloc_seg_a(1024*sizeof(uint32_t), 4096);
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
	
	uint32_t* page_table = (uint32_t*)(page_directory[directory_entry] & PAGE_DIR_PAGETABLE_MASK);
	page_table[pagetable_entry] = 
			PAGE_ENTRY_PRESENT|
			PAGE_ENTRY_BASE(physical_address)|
			flags;

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

