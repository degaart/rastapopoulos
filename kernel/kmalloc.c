#include <stdint.h>
#include "kmalloc.h"
#include "kutil.h"
#include "kterm.h"
#include "ll.h"

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

uint16_t* initial_memmap_size = (uint16_t*)0x502;
uint32_t memmap_size;
struct MEMMAP_ENTRY* initial_memmap = (struct MEMMAP_ENTRY*)0x508;
struct MEMMAP_ENTRY* memmap;

struct MEMBLOCK {
    uint32_t start;
    uint32_t end;
    struct MEMBLOCK* next;
};
LL_DECLARE(MEMBLOCK_LIST, struct MEMBLOCK);
LL_IMPLEMENT(MEMBLOCK_LIST, struct MEMBLOCK)
MEMBLOCK_LIST memblocks;

/*
	Allocate, using specified alignment
*/
void* kmalloc_seg_a(unsigned size, unsigned alignment) {
	static void* mem_start = (void*)0xFFFFFFFF;
	if(mem_start == (void*)0xFFFFFFFF)
		mem_start = kernel_end;

	/* Begin to allocate memory at mem_start, and align to element size */
	void* allocated_mem = ALIGN(mem_start, alignment);
	if(size >= 0x100000) {
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

void kmalloc_init() {
	memmap = initial_memmap;
	memmap_size = *initial_memmap_size;

    /* Sort memmap by base address */
    struct MEMMAP_ENTRY* new_memmap = (struct MEMMAP_ENTRY*)kmalloc_seg(memmap_size, sizeof(struct MEMMAP_ENTRY));
    struct MEMMAP_ENTRY* new_memmap_pointer = new_memmap;
    while(1) {
        /* find lowest entry */
        int lowest = UINT32_MAX;
        for(int i=0; i<memmap_size; i++) {
            if((memmap[i].base != UINT32_MAX) && (memmap[i].size != UINT32_MAX)) {
                if((lowest == UINT32_MAX) || (memmap[i].base < memmap[lowest].base)) {
                    lowest = i;
                }
            }
        }
        
        if(lowest == UINT32_MAX)
            break;
        
        new_memmap_pointer->base = memmap[lowest].base;
        new_memmap_pointer->size = memmap[lowest].size-1;
        new_memmap_pointer->type = memmap[lowest].type;
        new_memmap_pointer->type2 = memmap[lowest].type2;
        new_memmap_pointer++;
        memmap[lowest].base = UINT32_MAX;
        memmap[lowest].size = UINT32_MAX;
    }
    memmap = new_memmap;
    
    /* Remove entries that refer to high memory */
    new_memmap = (struct MEMMAP_ENTRY*)kmalloc_seg((memmap_size*2)+1, sizeof(struct MEMMAP_ENTRY));
    new_memmap_pointer = new_memmap;
    for(int i=0; i<memmap_size; i++) {
        if(memmap[i].base < UINT32_MAX) {
            if(memmap[i].base+memmap[i].size > UINT32_MAX)
                memmap[i].size = UINT32_MAX - memmap[i].base;
            new_memmap_pointer->base = memmap[i].base;
            new_memmap_pointer->size = memmap[i].size;
            new_memmap_pointer->type = memmap[i].type;
            new_memmap_pointer->type2 = memmap[i].type2;
            new_memmap_pointer++;
        }
    }
    memmap = new_memmap;
    memmap_size = new_memmap_pointer - new_memmap;

    /*
        Check for overlapping regions
        If they're of the same type, we just adjust them
        Else we bail out 'cause handling them would be too complicated. And I'm tired
    */
    for(int i=1; i<memmap_size; i++) {
        if(memmap[i].base < memmap[i-1].base+memmap[i-1].size) {
            if(memmap[i].type != memmap[i-1].type) {
                PANIC("Overllaping regions of memory are not implemented yet");
            }
            memmap[i-1].size = memmap[i].base - memmap[i-1].base - 1;
        }
    }
    
    /* Using base and extent of adjacent entries, add gaps in layout */
    new_memmap = (struct MEMMAP_ENTRY*)kmalloc_seg((memmap_size*2)+1, sizeof(struct MEMMAP_ENTRY));
    new_memmap_pointer = new_memmap;
    for(int i=0; i<memmap_size; i++) {
        uint64_t last_extent;
        if(i==0)
            last_extent = -1;
        else
            last_extent = memmap[i-1].base+memmap[i-1].size;
        
        if(memmap[i].base > last_extent+1) {
            new_memmap_pointer->base = last_extent+1;
            new_memmap_pointer->size = memmap[i].base-last_extent-2;
            new_memmap_pointer->type = MEMMAP_TYPE_GAP;
            new_memmap_pointer->type2 = 0;
            new_memmap_pointer++;
        }
        new_memmap_pointer->base = memmap[i].base;
        new_memmap_pointer->size = memmap[i].size;
        new_memmap_pointer->type = memmap[i].type;
        new_memmap_pointer->type2 = memmap[i].type2;
        new_memmap_pointer++;
    }
    
    /* We have yet to handle the last gap at end of memory (if needed) */
    struct MEMMAP_ENTRY* last_entry = new_memmap_pointer-1;
    if(last_entry->base+last_entry->size != UINT32_MAX) {
        new_memmap_pointer->base = last_entry->base+last_entry->size+1;
        new_memmap_pointer->size = UINT32_MAX - new_memmap_pointer->base;
        new_memmap_pointer->type = MEMMAP_TYPE_GAP;
        new_memmap_pointer->type2 = 0;
        new_memmap_pointer++;
    }
    memmap = new_memmap;
    memmap_size = new_memmap_pointer - new_memmap;
    
    write_string("Sorted gapless memory map:\n");
    for(int i=0; i<memmap_size; i++) {
    	write_string("    ");
    	write_uint32(memmap[i].base);
    	write_string(" - ");
    	write_uint32(memmap[i].base+memmap[i].size);
    	write_string(" (");
    	write_uint32(memmap[i].type);
    	write_string(")\n");
    }
    
    /* Verify memory map */
    ASSERT(memmap[0].base == 0);
    ASSERT(memmap[memmap_size-1].base+memmap[memmap_size-1].size == UINT32_MAX);
    for(int i=0; i<memmap_size; i++) {
        ASSERT(memmap[i].base < UINT32_MAX);
        ASSERT(memmap[i].size <= UINT32_MAX);
        ASSERT(memmap[i].base < memmap[i].base+memmap[i].size);

        if(i > 0) {
	        ASSERT(memmap[i-1].base+memmap[i-1].size+1 == memmap[i].base);
        }
    }
    
    /* Mark reserved memory and gaps as allocated */
    MEMBLOCK_LIST_init(&memblocks);
    for(int i=0; i<memmap_size; i++) {
        if(memmap[i].type != MEMMAP_TYPE_FREE) {
            struct MEMBLOCK* block = (struct MEMBLOCK*)kmalloc_seg(1, sizeof(struct MEMBLOCK));
            block->start = (uint32_t)memmap[i].base;
            block->end = (uint32_t)(memmap[i].base+memmap[i].size);
            MEMBLOCK_LIST_append(&memblocks, block);
        }
    }
    
    /* Mark entire low memory as allocated, for simplicity's sake */
    struct MEMBLOCK* block = (struct MEMBLOCK*)kmalloc_seg(1, sizeof(struct MEMBLOCK));
    block->start = 0x000000000;
    block->end = 0x100000-1;
    write_string("Initial allocated memory:\n");
    for(struct MEMBLOCK* block=memblocks.first; block; block=block->next) {
    	write_string("    ");
    	write_uint32(block->start);
    	write_string(" - ");
    	write_uint32(block->end);
    	write_string("\n");
    }
    
    _halt();
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

