#include <stdint.h>
#include "kmalloc.h"
#include "kutil.h"
#include "kterm.h"
#include "ll.h"
#include "kstring.h"

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
static uint32_t memmap_size;
static struct MEMMAP_ENTRY* initial_memmap = (struct MEMMAP_ENTRY*)0x508;
static struct MEMMAP_ENTRY* memmap;

/*
	Allocate, using specified alignment
*/
void* kmalloc_seg_a(unsigned size, unsigned alignment) {
	static void* mem_start = (void*)0xFFFFFFFF;
	if(mem_start == (void*)0xFFFFFFFF)
		mem_start = kernel_end;

	/* Begin to allocate memory at mem_start, and align to element size */
	void* allocated_mem = ALIGN(mem_start, alignment);
	if(size >= 0x200000) {
		write_string("WARNING: Trying to allocate ");
		write_uint32(size);
		write_string(" of memory in initial memory manager\n");
	}
	ASSERT((uint32_t)allocated_mem < 0x400000);
	
	/* Now, update mem_start to end of allocated memory */
	mem_start = allocated_mem + size + 1;
	return(allocated_mem);
}

/*
	Very simple kernel allocator, does not permit
	freeing of the allocated memory
	Should only be used when paging disabled
*/
void* kmalloc_seg(unsigned el_count, unsigned el_size) {
	return(kmalloc_seg_a(el_count*el_size, el_size));
}

/*
	Dump memory map given to us by bootloader
*/
void kmalloc_dump_memmap() {
	struct MEMMAP_ENTRY* entry = initial_memmap;
	for(int i=0; i<*initial_memmap_size; i++) {
		write_string("    BASE: ");
		write_uint32(entry->base);
		write_string(" LENGTH: ");
		write_uint32(entry->size);
		write_string(" TYPE: ");
		write_uint32(entry->type);
		write_string("\n");
		entry++;
	}	
}

#define KERNEL_MEMORY_SIZE 0x400000
uint16_t kernel_memory_bitmap[KERNEL_MEMORY_SIZE/16];

void kmalloc_init() {
	memset(kernel_memory_bitmap, 0, sizeof(kernel_memory_bitmap));
}

/* Set the value of a specific block */
void kmalloc_set_block(uint32_t block, uint32_t value) {
	int offset = block / 16;
	int bit = block % 16;

	if(value)	
		kernel_memory_bitmap[offset] |= (1<<bit);
	else
		kernel_memory_bitmap[offset] &= ~(1<<bit);
}

/* Get value for specific block */
int kmalloc_get_block(uint32_t block) {
	int offset = block / 16;
	int bit = block % 16;
	
	return( kernel_memory_bitmap[offset] & (1<<bit) );
}



