#include "pmm.h"
#include "debug.h"
#include "../bootldr/kernel_params.h"

#define REGION_USABLE 1
#define REGION_RESERVED 2
#define REGION_ACPI_RECLAIM 3

LinkedList<PMM::MemRegion> PMM::_regions;

void PMM::init(const void* bios_memmap, unsigned bios_memmap_size) {
    /* Init memmap */
    bios_memmap_t* regions = (bios_memmap_t*)bios_memmap;
    for(unsigned i=0; i<bios_memmap_size; i++) {
        if(regions[i].base_hi == 0 && regions[i].flags == REGION_USABLE) {
            add_region(regions[i].base_lo, regions[i].size_lo);
        }
    }

    for(auto region = _regions.iterator(); !region.end(); region.next()) {
        for(uint32_t page = region->base(); page < region->base() + region->size();page += PAGE_SIZE) {
            if(region->page_reserved(page)) {
                TRACE("WARNING: page 0x%X reserved!", page);
            }
            assert(!region->page_reserved(page));
        }
    }
}

void PMM::dump() {
    for(auto region = _regions.iterator(); !region.end(); region.next()) {
        TRACE("\tbase: 0x%X limit: 0x%X size: 0x%X", region->base(), region->base() + region->size() - 1, region->size());
    }
}

void PMM::dump_zones() {
    for(auto region = _regions.iterator(); !region.end(); region.next()) {
        uint32_t page = region->base();
        uint32_t zone = page;
        bool zone_reserved = region->page_reserved(zone);
        while(page < region->base() + region->size()) {
            if(region->page_reserved(page) != zone_reserved) {
                TRACE("\t0x%X - 0x%X %s", zone, page - 1, zone_reserved ? "reserved" : "free");
                zone = page;
                zone_reserved = region->page_reserved(page);
            }
            page += PAGE_SIZE;
        }
        TRACE("\t0x%X - 0x%X %s", zone, page - 1, zone_reserved ? "reserved" : "free");
    }
}

void PMM::reserve(uint32_t page) {
    assert((page % PAGE_SIZE) == 0);
    
    /* Find region containing page */
    for (auto region = _regions.iterator(); !region.end(); region.next()) {
        if (region->contains_page(page)) {
            region->reserve(page);
        }
    }
}

void PMM::add_region(uint32_t base, uint32_t size) {
    _regions.append(MemRegion(base, size));
}

uint32_t PMM::alloc() {
    for(auto region = _regions.iterator(); region.valid(); region.next()) {
        uint32_t page_frame;
        bool got_frame = region->find(&page_frame);
        if(got_frame) {
            region->reserve(page_frame);
            return page_frame;
        }
    }
    PANIC("Physical memory exhaustion");
    return 0;
}

uint32_t PMM::pages_total() {
    uint32_t total = 0;
    for(auto i = _regions.iterator(); i.valid(); i.next())
        total += i->pages();
    return total;
}

uint32_t PMM::pages_free() {
    uint32_t total = 0;
    for(auto i = _regions.iterator(); i.valid(); i.next())
        total += i->pages_free();
    return total;
}

