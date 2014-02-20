#include <stdint.h>
#include "bitmap.h"
#include "ll.h"
#include "kmalloc.h"
#include "kutil.h"
#include "kstring.h"
#include "pmm.h"

struct BITMAP physical_memory_map;

#define BIOS_MEMMAP_TYPE_FREE 			1
#define BIOS_MEMMAP_TYPE_RESERVED 		2
#define BIOS_MEMMAP_TYPE_ACPI_RECLAIM 	3
#define BIOS_MEMMAP_TYPE_ACPI_NVS 		4
struct BIOS_MEMMAP_ENTRY {
	uint64_t base;
	uint64_t size;
	uint32_t type;
	uint32_t type2;
}__attribute__((packed));

static uint16_t* bios_memmap_size = (uint16_t*)0x502;
static struct BIOS_MEMMAP_ENTRY* bios_memmap = (struct BIOS_MEMMAP_ENTRY*)0x508;

struct MEM_REGION {
	uint32_t base;
	struct BITMAP* bitmap;
    struct MEM_REGION* next;
#ifdef DEBUG
    uint32_t size;
#endif
} __attribute__((packed));

LL_DECLARE(MEM_REGIONS, struct MEM_REGION);
LL_IMPLEMENT(MEM_REGIONS, struct MEM_REGION);

MEM_REGIONS mem_regions;

/*
 We shall store usable physical memory in a linked
 list
 An entry contains the base of the memory and a bitmap
 of it's allocation
 We define a 4096 bytes block as a page
 */
void pmm_init() {
    /* init memory regions structure */
	MEM_REGIONS_init(&mem_regions);
	for(int i=0; i<*bios_memmap_size; i++) {
        if((bios_memmap[i].type == BIOS_MEMMAP_TYPE_FREE) && (bios_memmap[i].base < UINT32_MAX) && (bios_memmap[i].base+bios_memmap[i].size < UINT32_MAX)) {
            struct MEM_REGION* region = (struct MEM_REGION*)kmalloc_seg(1, sizeof(struct MEM_REGION));
            region->base = (bios_memmap[i].base & UINT32_MAX);
            
            uint32_t true_size = bios_memmap[i].size & UINT32_MAX;
            uint32_t bitmap_size = bitmap_get_storage_size(true_size/4096);
            struct BITMAP* region_bitmap = (struct BITMAP*)kmalloc_seg(1, sizeof(struct BITMAP));
            bitmap_init(region_bitmap, true_size/4096, kmalloc_seg(bitmap_size, 1));
            region->bitmap = region_bitmap;
#ifdef DEBUG
            region->size = true_size;
#endif
            
            MEM_REGIONS_append(&mem_regions, region);
		}
	}
}

/*
    Allocate a new page from physical memory
    Params: size in bytes
    Returns the physical location of the page, or UINT32_MAX if there's no memory left
 */
uint32_t pmm_alloc_page() {
    /*
        Iterate MEM_REGIONS, searching for a free page
    */
    for(struct MEM_REGION* region = mem_regions.first; region; region = region->next) {
        uint32_t location = bitmap_find_free(region->bitmap);
        
        if(location != UINT32_MAX ) {
            /* Mark as allocated */
            ASSERT(!bitmap_get(region->bitmap, location));
            
            bitmap_set(region->bitmap, location, 1);
            uint32_t result = region->base+(location*4096);
            
            ASSERT(result < (region->base+region->size));
            return(result);
        }
        
        
    }
    return(UINT32_MAX);
}

/* Free a page in the specified location */
void pmm_free(uint32_t location) {
    if(location % 4096)
        PANIC("location is not divisible by 4096\n");

    for(struct MEM_REGION* region = mem_regions.first; region; region = region->next) {
        if((location >= region->base) && (location < region->base+region->size)) {
            if(!bitmap_get(region->bitmap, (location-region->base)/4096)) {
                PANIC("Trying to deallocate an unallocated page\n");
            }
            bitmap_set(region->bitmap, (location-region->base)/4096, 0);
            return;
        }
    }
    PANIC("location not found!\n");
}

/*
 Allocate a contiguous region of memory
 Returns UINT32_MAX if there's no memory left
 */
uint32_t pmm_alloc_range(uint32_t pages) {
    if(!pages)
        PANIC("Trying to allocate a range of 0 pages\n");
    
    for(struct MEM_REGION* region = mem_regions.first; region; region = region->next) {
        uint32_t location = bitmap_find_free_region(region->bitmap, pages);
        if(location != UINT32_MAX) {
            for(unsigned i=location; i<location+pages; i++) {
                if(bitmap_get(region->bitmap, i))
                    PANIC("Page is already allocated\n");
                bitmap_set(region->bitmap, i, 1);
            }
            uint32_t result = region->base+(location*4096);
            return(result);
        }
    }
    
    return(UINT32_MAX);
}

/*
 Free a contiguous region of memory
 range: count of pages to free
*/
void pmm_free_range(uint32_t location, uint32_t range) {
    for(struct MEM_REGION* region = mem_regions.first; region; region = region->next) {
        if((location>=region->base) && (location<region->base+region->size)) {
            if(location+(range*4096) >= region->base+region->size)
                PANIC("Range of pages to free extends beyond current region\n");

            for(unsigned page=(location - region->base)/4096; page<((location - region->base)/4096) + range; page++) {
                if(!bitmap_get(region->bitmap, page))
                    PANIC("Page is not allocated\n");
                bitmap_set(region->bitmap, page, 0);
            }
        }
    }
}

/*
    Get total free memory
*/
uint32_t pmm_get_free() {
    uint32_t free_memory = 0;
    for(struct MEM_REGION* region = mem_regions.first; region; region = region->next) {
        for(unsigned page=0; page<region->bitmap->bitcount; page++) {
            if(!bitmap_get(region->bitmap, page))
                free_memory += 4096;
        }
    }
    return(free_memory);
}

