#include <stdint.h>
#include "kmalloc.h"
#include "kutil.h"
#include "kterm.h"
#include "ll.h"
#include "kstring.h"
#include "vmm.h"

struct FREE_BLOCK {
	LL_HEADER(FREE_BLOCK); 	/* Linked list item header */
	void* base;				/* Linear address of the start of the block */
	uint32_t size;			/* Size of the block in bytes */
};
LL_DECLARE(FREE_BLOCKS, FREE_BLOCK);
LL_IMPLEMENT(FREE_BLOCKS, FREE_BLOCK);

struct FREE_BLOCKS free_blocks;		/* List of free blocks */
static uint32_t heap_extent;		/* First byte of unusable memory */

/*
	Register a new block in the free list	
*/
static struct FREE_BLOCK* kmalloc_register_block(void* linear_address, uint32_t size) {
	struct FREE_BLOCK* block = (struct FREE_BLOCK*)kmalloc(sizeof(struct FREE_BLOCK*));
	block->base = linear_address;
	block->size = size;
	return(block);
}

static void kmalloc_remove_block(struct FREE_BLOCK* block) {
	FREE_BLOCKS_remove(&free_blocks, block);
}

static void kmalloc_dump() {
	for(struct FREE_BLOCK* block=free_blocks.first; block; block=block->next) {
		write_format("    %X - %X (%u bytes)\n", block->base, block->base+block->size, block->size);
	}	
}

void kmalloc_init() {
	heap_extent = (uint32_t)kmalloc_seg_get_start();
	FREE_BLOCKS_init(&free_blocks);
	
	/*
		Allocate a page right away from vmm, as kmalloc uses dynamic allocation
		We can place the new block in the newly allocated page
	*/
	struct FREE_BLOCK* block = vmm_alloc_pages(1);
	block->base = block+4;
	block->size = 4096-4;
	FREE_BLOCKS_append(&free_blocks, block);
}

/*
	Allocate memory of the given size
*/
void* kmalloc(uint32_t size) {
	TRACE("Requesting %u bytes", size);

	/* adjust size for header */
	DUMP32D(size);
	DUMP32D(size+4);
	DUMP32D(14686+4);
	size += 4;
	DUMP32D(size);
	
	/* Find fitting free block */
	struct FREE_BLOCK* block = 0;
	for(block=free_blocks.first; block; block = block->next) {
		if(block->size >= size)
			break;
	}
	
	/*
		if no free blocks found to satisfy request, ask
		VMM for more memory
	*/
	if(!block) {
		DUMP32D(size);
		DUMP32D(ALIGN32(size, 4096));
		DUMP32D(ALIGN32(size, 4096)/4096);
		block = kmalloc_register_block(
			vmm_alloc_pages(ALIGN32(size, 4096)/4096),
			ALIGN32(size,4096)
		);
	}

	/* Adjust start of block */
	void* location = block->base;
	block->base += size;
	block->size -= size;
	
	/* If block exhausted, we can remove if from free pool */
	if(!block->size) {
		kmalloc_remove_block(block);
	}
	
	/* Push size of block into the header */
	*((uint32_t*)location) = size;
	
	kmalloc_dump();
	return(location);
}

/*
	Allocate, using specified alignment
*/
static void* kmalloc_seg_mem_start = (void*)0xFFFFFFFF;
void* kmalloc_seg_a(unsigned size, unsigned alignment) {
	if(vmm_paging_enabled())
		PANIC("Calling kmalloc_seg_a with pagin enabled");

	if(kmalloc_seg_mem_start == (void*)0xFFFFFFFF)
		kmalloc_seg_mem_start = kernel_end;

	/* Begin to allocate memory at mem_start, and align to element size */
	void* allocated_mem = ALIGN(kmalloc_seg_mem_start, alignment);
	if(size >= 0x100000) {
		write_string("WARNING: Trying to allocate ");
		write_uint32(size);
		write_string(" of memory in initial memory manager\n");
	}
	ASSERT((uint32_t)allocated_mem < 0x400000);
	
	/* Now, update mem_start to end of allocated memory */
	kmalloc_seg_mem_start = allocated_mem + size + 1;
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
	Get start of the current zone
*/
void* kmalloc_seg_get_start() {
	return(kmalloc_seg_mem_start);
}


