#ifndef _PMM_H_
#define _PMM_H_

#include <stdint.h>
#include "linked_list.h"
#include "bitset.h"

/*
	Physical memory manager
	Only manages physical memory
*/
class PMM {
private:
    struct MemRegion {
    private:
        uint32_t _base;
        uint32_t _size;
        Bitset _bitset;
        uint32_t _free_size;
    public:
        MemRegion(uint32_t base, uint32_t size);
        MemRegion(const MemRegion&);
        MemRegion& operator=(const MemRegion&) = delete;
        
        uint32_t base() const;
        uint32_t size() const;          /* size in bytes of region */
        uint32_t pages() const;         /* number of pages in region */
        uint32_t pages_free() const;    /* Number of free pages in region */
        
        /* Checks if given page is inside region */
        bool contains_page(uint32_t page) const;
        
        /* Checks if given page is reserved, throws error if page already reserved, or base outside region */
        bool page_reserved(uint32_t page) const;
        
        /* Reserves given page, throws error if page already reserved, or page outside region */
        void reserve(uint32_t page);
        
        /* Frees given page, throws error if page already free, or page outside region */
        void free(uint32_t page);
        
        /* Get index of given page inside bitset */
        unsigned indexof(uint32_t page) const;
        
        /* Find free page. On return, page points to physical address of free page */
        bool find(uint32_t* page) const;
        
        /* Find free range of pages. On return, page points to physical address of first free page; Size is in bytes */
        bool find(uint32_t* page, unsigned size) const;
    };
    
    static LinkedList<MemRegion> _regions;
    static void add_region(uint32_t base, uint32_t size);
public:
    static const int PAGE_SIZE = 4096;
    
	static void init(const void* bios_memmap, unsigned bios_memmap_size);
    static void dump();
    static void dump_zones();

	/*
		Mark a specific physical page as allocated
        page: address of page frame
	*/
	static void reserve(uint32_t page);

	/*
		Finds a free page, reserve, and return it
        If not free pages, panics
	*/
	static uint32_t alloc();

	/*
		Mark a specific page as free
        page: address of page frame
	*/
	static void free(uint32_t page);
    
    /*
        Returns total number of pages
    */
    static uint32_t pages_total();

    /*
        Returns number of free pages
     */
    static uint32_t pages_free();

};

#endif

