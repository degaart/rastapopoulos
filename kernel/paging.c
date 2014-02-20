#include <stdint.h>
#include "kutil.h"
#include "paging.h"
#include "kmalloc.h"
#include "kstring.h"
#include "bitmap.h"

struct BITMAP physical_memory_map;

#define MEMMAP_TYPE_GAP             0
#define MEMMAP_TYPE_FREE 			1
#define MEMMAP_TYPE_RESERVED 		2
#define MEMMAP_TYPE_ACPI_RECLAIM 	3
#define MEMMAP_TYPE_ACPI_NVS 		4
struct MEMMAP_ENTRY {
	uint64_t base;
	uint64_t size;
	uint32_t type;
	uint32_t type2;
}__attribute__((packed));

static uint16_t* initial_memmap_size = (uint16_t*)0x502;
static struct MEMMAP_ENTRY* initial_memmap = (struct MEMMAP_ENTRY*)0x508;

/*
	Initialize paging by mapping the currently used kernel memory
*/
void paging_init() {
	/*
		First, we initialize our physical_memory_map using information from BIOS
		Our physical memory bitmap will take up 1Mb
	*/
	const unsigned physical_memory_bitcount = 4*1024*256;
	unsigned physical_memory_map_size = bitmap_get_storage_size(physical_memory_bitcount);		/* There are 256 4Kb pages in 1Mb */
	bitmap_init(physical_memory_map, physical_memory_bitcount, kmalloc_seg_a(physical_memory_map_size, 4));
	for(int i=0; i<*initial_memmap_size; i++) {
		if(initial_memmap[i].type == MEMMAP_TYPE_FREE) {
			/* Mark every page contained in this entry as allocated */
			
			
			
		}
	}

	uint32_t* page_table = (uint32_t*)kmalloc_seg_a(1024*sizeof(uint32_t), 4096);
	
	/* Map first 4Mb for kernel */
	uint32_t page_start = 0;
	for(int i=0; i<1024; i++) {
		page_table[i] = 
			PAGE_ENTRY_PRESENT|
			PAGE_ENTRY_SUPERVISOR|
			PAGE_DIR_BASE(page_start);
		page_start += 4096;
	}
	
	uint32_t* page_directory = (uint32_t*)kmalloc_seg_a(1024*sizeof(uint32_t), 4096);
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



