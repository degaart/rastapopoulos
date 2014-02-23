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

#define BLOCK_MAGIC 0xB16B00B5
struct BLOCK_HEADER {
    uint32_t magic;
    uint32_t size;
};
#define BLOCK_HEADER_SIZE sizeof(struct BLOCK_HEADER)

static uint32_t heap_size = 0;

/*
    Ask VMM for more pages, and add these pages to the free list
 */
struct FREE_BLOCK* kmalloc_add_free_pages(uint32_t size) {
    int pages_count = ALIGN32(size+sizeof(struct FREE_BLOCK), 4096)/4096;
    /* TRACE("Growing kernel heap by %u bytes", pages_count*4096); */

	TRACE("Allocating %u pages", pages_count);
	struct FREE_BLOCK* block = vmm_alloc_pages(pages_count);
	ASSERT(vmm_linear_to_physical(block) != 0);
	/* ASSERT(block != 0x400000); */
	((uint8_t*)block)[0] = 0;

	block->base = ((void*)block)+sizeof(struct FREE_BLOCK); /* page fault here */
	block->size = (pages_count*4096)-sizeof(struct FREE_BLOCK);
	FREE_BLOCKS_append(&free_blocks, block);
    heap_size += (pages_count*4096);
    return(block);
}

void kmalloc_remove_block(struct FREE_BLOCK* block) {
	FREE_BLOCKS_remove(&free_blocks, block);
}

void kmalloc_dump() {
	for(struct FREE_BLOCK* block=free_blocks.first; block; block=block->next) {
#ifdef __APPLE__
        printf("    %08X - %08X (%u bytes)\n", (uint32_t)block->base, (uint32_t)(block->base+block->size), block->size);
#else
		write_format("    %X - %X (%u bytes)\n", block->base, block->base+block->size, block->size);
#endif
	}
}

void kmalloc_init() {
	FREE_BLOCKS_init(&free_blocks);
}

/*
 Allocate memory of the given size
 */
void* kmalloc(uint32_t size) {
	/* adjust size for header */
	size += BLOCK_HEADER_SIZE;
	
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
        block = kmalloc_add_free_pages(size);
        ASSERT(block != NULL);
        ASSERT(block->base != NULL);
        ASSERT(block->size >= size);
	}
    
	/* Adjust start of block */
    ASSERT(block->base != NULL);
    ASSERT(block->size >= size);

	void* location = block->base;
	block->base += size;
	block->size -= size;
	
	/* If block exhausted, we can remove if from free pool */
	if(!block->size) {
		kmalloc_remove_block(block);
	}
	
	/* Push size of block into the header */
    struct BLOCK_HEADER* header = (struct BLOCK_HEADER*)location;
    header->magic = BLOCK_MAGIC;
    header->size = size - BLOCK_HEADER_SIZE;
	return(location + BLOCK_HEADER_SIZE);
}

void kfree(void* location) {
    struct BLOCK_HEADER* header = location - BLOCK_HEADER_SIZE;
    ASSERT(header->magic == BLOCK_MAGIC);
   
    /* Get block size from header */
    uint32_t size = header->size;
    
    /* Adjust size and location to take care of header */
    size += BLOCK_HEADER_SIZE;
    location -= BLOCK_HEADER_SIZE;
    
    /*
        Check if we can merge this block with other blocks
     */
    struct FREE_BLOCK* block;
    for(block=free_blocks.first; block; block=block->next) {
        if(block->base == location+size) {
            block->base = location;
            block->size += size;
            return;
        } else if(block->base+block->size == location) {
            block->size += size;
            return;
        }
    }
    
    /* Check if we can reuse the blocks memory to store it's FREE_BLOCK node */
    if(size > sizeof(struct FREE_BLOCK)) {
        block = location;
        block->base = location+sizeof(struct FREE_BLOCK);
        block->size = size - sizeof(struct FREE_BLOCK);
        FREE_BLOCKS_append(&free_blocks, block);
    } else {
        block = (struct FREE_BLOCK*)kmalloc(sizeof(struct FREE_BLOCK));
        block->base = location;
        block->size = size;
        FREE_BLOCKS_append(&free_blocks, block);
    }
}

/*
    Query largest contiguous block that can be allocated
 */
uint32_t kmalloc_largest() {
    uint32_t largest = 0;
    for(struct FREE_BLOCK* block = free_blocks.first; block; block=block->next) {
        if((block->size > BLOCK_HEADER_SIZE) && (block->size - BLOCK_HEADER_SIZE > largest))
            largest = block->size - BLOCK_HEADER_SIZE;
    }
    return(largest);
}

/*
    Return size of kernel heap
 */
uint32_t kmalloc_heap_size() {
    return(heap_size);
}

#ifndef __APPLE__
/*
 Allocate, using specified alignment
 */
static void* kmalloc_seg_mem_start = (void*)0xFFFFFFFF;
void* kmalloc_seg_a(unsigned size, unsigned alignment) {
	if(vmm_paging_enabled())
		PANIC("Calling kmalloc_seg_a with paging enabled");
    
	if(kmalloc_seg_mem_start == (void*)0xFFFFFFFF)
		kmalloc_seg_mem_start = kernel_end;
    
	/* Begin to allocate memory at mem_start, and align to element size */
	void* allocated_mem = ALIGN(kmalloc_seg_mem_start, alignment);
	if(size >= 0x100000) {
		TRACE("WARNING: Trying to allocate %u bytes of memory in initial memory manager", size);
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
#endif

