#include <stdint.h>
#include "bitmap.h"
#include "ll.h"
#include "kmalloc.h"
#include "kutil.h"
#include "kstring.h"
#include "pmm.h"
#include "vmm.h"
#include "kterm.h"

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
	LL_HEADER(MEM_REGION);
	uint32_t base;
	struct BITMAP* bitmap;
    uint32_t size;
    uint32_t type;
} __attribute__((packed));

LL_DECLARE(MEM_REGIONS, MEM_REGION);
LL_IMPLEMENT(MEM_REGIONS, MEM_REGION);

static struct MEM_REGIONS mem_regions;

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
        if((bios_memmap[i].base < UINT32_MAX) && (bios_memmap[i].base+bios_memmap[i].size < UINT32_MAX)) {
    		if(bios_memmap[i].type != BIOS_MEMMAP_TYPE_FREE && bios_memmap[i].type != BIOS_MEMMAP_TYPE_RESERVED && bios_memmap[i].type != BIOS_MEMMAP_TYPE_ACPI_RECLAIM)
    			continue;
            struct MEM_REGION* region = (struct MEM_REGION*)kmalloc_seg(1, sizeof(struct MEM_REGION));
            region->base = (bios_memmap[i].base & UINT32_MAX);
            
            uint32_t true_size;
            if(bios_memmap[i].type == BIOS_MEMMAP_TYPE_FREE || bios_memmap[i].type == BIOS_MEMMAP_TYPE_ACPI_RECLAIM)
            	true_size = ((bios_memmap[i].size & UINT32_MAX)/PAGE_SIZE)*PAGE_SIZE; 	/* Makes us oblivious to memory < PAGE_SIZE at the end of the block */
            else
            	true_size = ALIGN32(bios_memmap[i].size & UINT32_MAX, PAGE_SIZE);					/* We don't care about memory memory access issues, so we just align size to 4096 bytes */
            
            ASSERT(true_size != 0);

            uint32_t bitmap_size = bitmap_get_storage_size(true_size/PAGE_SIZE);
            TRACE("size: %d, bitmap_size: %d", bitmap_size, true_size/PAGE_SIZE);

            struct BITMAP* region_bitmap = (struct BITMAP*)kmalloc_seg(1, sizeof(struct BITMAP));
            bitmap_init(region_bitmap, true_size/PAGE_SIZE, kmalloc_seg(bitmap_size, 4));
            if(bios_memmap[i].type == BIOS_MEMMAP_TYPE_ACPI_RECLAIM)
            	bitmap_fill(region_bitmap);

            region->bitmap = region_bitmap;
            region->size = true_size;
            if(bios_memmap[i].type == BIOS_MEMMAP_TYPE_RESERVED)
            	region->type = REGION_RESERVED;
            else
	            region->type = REGION_FREE;
            
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
    	if(region->type == REGION_FREE) {
			uint32_t location = bitmap_find_free(region->bitmap);
			
			if(location != UINT32_MAX ) {
				/* Mark as allocated */
				ASSERT(!bitmap_get(region->bitmap, location));
				
				bitmap_set(region->bitmap, location, 1);
				uint32_t result = region->base+(location*PAGE_SIZE);
	
				ASSERT(result < (region->base+region->size));
				return(result);
			}
        }
    }
    return(UINT32_MAX);
}

/* Free a page in the specified location */
void pmm_free_page(uint32_t location) {
    if(location % PAGE_SIZE)
        PANIC("location is not divisible by PAGE_SIZE\n");

    for(struct MEM_REGION* region = mem_regions.first; region; region = region->next) {
        if((location >= region->base) && (location < region->base+region->size)) {
            if(!bitmap_get(region->bitmap, (location-region->base)/PAGE_SIZE)) {
                PANIC("Trying to deallocate an unallocated page\n");
            }
            bitmap_set(region->bitmap, (location-region->base)/PAGE_SIZE, 0);
            return;
        }
    }
    PANIC("location not found!\n");
}

/*
 Allocate a contiguous region of memory
 Param: pages: number of pages to allocate
 Returns UINT32_MAX if there's no memory left
 */
uint32_t pmm_alloc(uint32_t pages) {
    if(!pages)
        PANIC("Trying to allocate a range of 0 pages\n");
    
    for(struct MEM_REGION* region = mem_regions.first; region; region = region->next) {
    	if(region->type == REGION_FREE) {
			uint32_t location = bitmap_find_free_region(region->bitmap, pages);
			if(location != UINT32_MAX) {
				for(unsigned i=location; i<location+pages; i++) {
					if(bitmap_get(region->bitmap, i))
						PANIC("Page is already allocated\n");
					bitmap_set(region->bitmap, i, 1);
				}
				uint32_t result = region->base+(location*PAGE_SIZE);
				return(result);
			}
        }
    }
    return(UINT32_MAX);
}

/*
 Free a contiguous region of memory
 range: count of pages to free
*/
void pmm_free(uint32_t location, uint32_t range) {
    for(struct MEM_REGION* region = mem_regions.first; region; region = region->next) {
        if((location>=region->base) && (location<region->base+region->size)) {
            if(location+(range*PAGE_SIZE) >= region->base+region->size)
                PANIC("Range of pages to free extends beyond current region\n");

            for(unsigned page=(location - region->base)/PAGE_SIZE; page<((location - region->base)/PAGE_SIZE) + range; page++) {
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
    	if(region->type == REGION_FREE) {
			for(unsigned page=0; page < region->bitmap->bitcount; page++) {
				if(!bitmap_get(region->bitmap, page))
					free_memory += PAGE_SIZE;
			}
        }
    }
    return(free_memory);
}

/*
	Dump memory map given to us by bootloader
*/
void pmm_dump_bios_memmap() {
	struct BIOS_MEMMAP_ENTRY* entry = bios_memmap;
	for(int i=0; i<*bios_memmap_size; i++) {
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

void pmm_dump_mem_regions() {
	for(struct MEM_REGION* region = mem_regions.first; region; region = region->next) {
		write_format("    %X - %X (%s)\n", region->base, region->base+region->size, (region->type == REGION_FREE)?"free":"reserved");
	}
}

/*
	Check if the page at specified location is usable
	
	WARNINGWARNINGWARNING: Ok, listen now. This is past you speaking.
	I know this is gonna bite you in the ass in the future. So listen
	carefully: a page which is usable may contain addresses which are
	memory-mapped physically to a device, e.g. the VGA bios. So don't
	go and make wild assumptions with this, ok?
*/
uint32_t pmm_page_usable(uint32_t location) {
	ASSERT((location % PAGE_SIZE) == 0);
	for(struct MEM_REGION* region = mem_regions.first; region; region = region->next) {
		if((location >= region->base) && (location < region->base + region->size)) {
			return(!bitmap_get(region->bitmap, (location - region->base)/PAGE_SIZE));
		}
	}
	return(0);
}

int pmm_page_status(uint32_t location) {
	ASSERT((location % PAGE_SIZE) == 0);
	for(struct MEM_REGION* region = mem_regions.first; region; region = region->next) {
		if((location >= region->base) && (location < region->base + region->size)) {
			if(region->type == REGION_RESERVED)
				return(PMM_STATUS_RESERVED);
			else if(bitmap_get(region->bitmap, (location - region->base)/PAGE_SIZE))
				return(PMM_STATUS_ALLOCATED);
			else
				return(PMM_STATUS_FREE);
		}
	}
	return(PMM_STATUS_ABSENT);
}

/*
	Reserve the page at specified location
*/
void pmm_reserve(uint32_t location) {
	for(struct MEM_REGION* region = mem_regions.first; region; region = region->next) {
		if((location >= region->base) && (location < region->base + region->size)) {
			if(bitmap_get(region->bitmap, (location - region->base)/PAGE_SIZE))
				PANIC("Trying to reserve already allocated memory at physical address %X", location);
			bitmap_set(region->bitmap, (location - region->base)/PAGE_SIZE, 1);
			return;
		}
	}
	
	PANIC("Trying to reserve memory which is absent at physical address %X", location);
}

/*
	Manually add a region
	Please. Please, do not add overlapping regions of different types
*/
void pmm_add_region(uint32_t base, uint32_t size, uint32_t type) {
	if(base % PAGE_SIZE)
		PANIC("Invalid region");
	if(size % PAGE_SIZE)
		PANIC("Invalid page size");
	if(vmm_paging_enabled())
		PANIC("Paging already enabled");
	
	/* Check if it's overlapping another existing memory region */
	for(struct MEM_REGION* region = mem_regions.first; region; region = region->next) {
		if(
			((base >= region->base) && (base <= region->base+region->size)) ||
			((region->base >= base) && (region->base <= base+size))
		) {
			PANIC("Overlapping regions: %X - %X and %X - %X", base, base+size, region->base, region->base+size);
		}
	}
	
	struct MEM_REGION* region = (struct MEM_REGION*)kmalloc_seg_a(sizeof(struct MEM_REGION), 4);
	region->base = base;
	region->size = size;
	region->type = type;
	region->bitmap = (struct BITMAP*)kmalloc_seg(1, sizeof(struct BITMAP*));
	bitmap_init(region->bitmap, size/PAGE_SIZE, kmalloc_seg_a(bitmap_get_storage_size(size/PAGE_SIZE), 4));

	MEM_REGIONS_append(&mem_regions, region);
}

