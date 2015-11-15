#include "pagedir.h"
#include "util.h"
#include "debug.h"
#include "string.h"
#include "pmm.h"
#include "kmalloc.h"
#include "vmm.h"

#define PAGE_DIRECTORY_INDEX(x) (((x) >> 22) & 0x3ff)
#define PAGE_TABLE_INDEX(x) (((x) >> 12) & 0x3ff)
#define PAGE_GET_PHYSICAL_ADDRESS(x) (*x & ~0xfff)

Pagedir::~Pagedir() {
    /*
        Free any pageframes with flag PTE_ALLOCATED
    */
    for(unsigned table = 0; table < 1024; table++) {
        if(_entries[table] & PDE_PRESENT) {
            for(uint32_t entry = 0; entry < 1024; entry++) {
                if(_tables[table]->entries[entry] & PTE_PRESENT) {
                    if(_tables[table]->entries[entry] & PTE_ALLOCATED) {
                        uint32_t pageframe = _tables[table]->entries[entry] & PTE_FRAME;
                        TRACE("Freeing pageframe 0x%X", pageframe);
                        PMM::free(pageframe);
                    }
                }
            }
            kfree(_tables[table]);
        }
    }
}

Pagedir* Pagedir::create() {
    Pagedir* dir = (Pagedir*)kmalloc_a(sizeof(Pagedir), PAGE_SIZE);
    bzero(dir->_entries, sizeof(dir->_entries));
    bzero(dir->_tables, sizeof(dir->_tables));

    bool got_pysical = VMM::get_physical(dir, &dir->_physical);
    assert(got_pysical);

    // TRACE("Created pagedir %p, physical 0x%X", dir, dir->_physical);
    return dir;
}

void Pagedir::map(uint32_t va, uint32_t pa, uint32_t flags) {
    if(va % PAGE_SIZE)
        PANIC("Invalid VA: 0x%X", va);
    if(pa % PAGE_SIZE)
        PANIC("Invalid PA: 0x%X", pa);

    uint32_t dir_index = PAGE_DIRECTORY_INDEX(va);
    uint32_t table_index = PAGE_TABLE_INDEX(va);

    pagetable_t* page_table = _tables[dir_index];
    if(!page_table || !(_entries[dir_index] & PTE_PRESENT)) {
        /* Kmalloc can't get physical address anymore now. We must do the grunt work of decoding pagetable to get physical address */
        page_table = (pagetable_t*)kmalloc_a(sizeof(pagetable_t), PAGE_SIZE);

        uint32_t table_physical;
        bool got_pysical = VMM::get_physical(page_table, &table_physical);
        assert(got_pysical);

        // TRACE("Allocated new pagetable: %p (physical 0x%X)", page_table, table_physical);
        bzero(page_table, sizeof(pagetable_t));

        // TODO: If flags contains PTE_USER, then enfore PDE_USER in pagedir entry too
        _entries[dir_index] = (table_physical & PDE_FRAME) | PDE_PRESENT | PDE_WRITABLE | PDE_USER;
        _tables[dir_index] = page_table;
    }

    if(page_table->entries[table_index] & PDE_PRESENT) {
        PANIC(
            "VA 0x%X already mapped to PA 0x%X, flags 0x%X (trying to remap to PA 0x%X, flags 0x%X)",
            va, pa,
            page_table->entries[table_index] & (~PTE_FRAME),
            pa, flags
        );
    }
    page_table->entries[table_index] = pa | flags;
}

void Pagedir::unmap(uint32_t va) {
    uint32_t dir_index = PAGE_DIRECTORY_INDEX(va);
    uint32_t table_index = PAGE_TABLE_INDEX(va);

    pagetable_t* page_table = _tables[dir_index];
    if(!page_table || !(_entries[dir_index] & PTE_PRESENT)) {
        PANIC("VA 0x%X not mapped", va);
    }

    if(!(page_table->entries[table_index] & PTE_PRESENT)) {
        PANIC("VA 0x%X not mapped", va);
    } else if(page_table->entries[table_index] & PTE_ALLOCATED) {
        PANIC("Please use dealloc() to unmap this page");
    }
    page_table->entries[table_index] &= ~PTE_PRESENT;
}

bool Pagedir::get_physical(uint32_t va, uint32_t* pa) {
    unsigned dir_index = PAGE_DIRECTORY_INDEX(va);
    if(_entries[dir_index] & PDE_PRESENT) {
        pagetable_t* pagetable = _tables[dir_index];
        unsigned table_index = PAGE_TABLE_INDEX(va);
        if(pagetable->entries[table_index] & PTE_PRESENT) {
            uint32_t frame = pagetable->entries[table_index] & PTE_FRAME;
            uint32_t offset = va & PTE_OFFSET;
            if(pa)
                *pa = frame + offset;
            return true;
        }
    }
    return false;
}

bool Pagedir::is_mapped(uint32_t va) {
    return get_physical(va, nullptr);
}

void Pagedir::set_physical(uint32_t physical) {
    _physical = physical;
}

uint32_t Pagedir::get_physical(uint32_t physical) {
    return _physical;
}

void Pagedir::copy_kernel_mappings(Pagedir* pagedir) {
    _entries[0] = pagedir->_entries[0];                 /* first entry guaranteed to be mapped */
    _tables[0] = pagedir->_tables[0];
    // TRACE("_physical: 0x%X, pagedir->_physical: 0x%X", _physical, pagedir->_physical);

    /* TODO: Handle remaining maps > 3Gb */
}

uint32_t Pagedir::physical() {
    return _physical;
}

uint32_t Pagedir::alloc(uint32_t va, uint32_t flags) {
    assert(!(flags & PTE_ALLOCATED));

    uint32_t page_frame = PMM::alloc();
    map(va, page_frame, flags | PTE_ALLOCATED);
    return page_frame;
}

void Pagedir::dealloc(uint32_t va) {
    uint32_t attr = get_page_attr(va, nullptr);
    if(!(attr & PTE_ALLOCATED))
        PANIC("Pageframe 0x%X was not allocated!", va);

    uint32_t page_frame;
    bool got_physical = get_physical((void*)va, &page_frame);
    assert(got_physical);
    PMM::free(page_frame);

    set_page_attr(va, attr & (~PTE_ALLOCATED));
    unmap(va);
}

bool Pagedir::check_readable_block(const void* va, size_t size, uint32_t* first_unreadable, uint32_t* first_invalid) {
    uint32_t first_page = truncate((uint32_t)va, PAGE_SIZE);
    uint32_t last_page = truncate( ((uint32_t)va) + size, PAGE_SIZE );

    if(first_unreadable)
        *first_unreadable = 0;
    if(first_invalid)
        *first_invalid = 0;

    for(uint32_t page = first_page; page <= last_page; page += PAGE_SIZE) {
        uint32_t dir_attr;
        uint32_t attr = get_page_attr(page, &dir_attr);
        if(!(dir_attr & PDE_PRESENT) || !(attr & PTE_PRESENT)) {
            if(first_invalid)
                *first_invalid = page;
            return false;
        } else if(!(dir_attr & PDE_USER) || !(attr & PTE_USER)) {
            if(first_unreadable)
                *first_unreadable = page;
            return false;
        }
    }
    return true;
}

bool Pagedir::check_writable_block(const void* va, size_t size, uint32_t* first_readonly, uint32_t* first_invalid) {
    uint32_t first_page = truncate((uint32_t)va, PAGE_SIZE);
    uint32_t last_page = truncate( ((uint32_t)va) + size, PAGE_SIZE );

    if(first_readonly)
        *first_readonly = 0;
    if(first_invalid)
        *first_invalid = 0;

    for(uint32_t page = first_page; page <= last_page; page += PAGE_SIZE) {
        uint32_t dir_attr;
        uint32_t attr = get_page_attr(page, &dir_attr);
        if(!(dir_attr & PDE_PRESENT) || !(attr & PTE_PRESENT)) {
            if(first_invalid)
                *first_invalid = page;
            return false;
        } else if(!(dir_attr & PDE_USER) || !(attr & PTE_USER) || !(dir_attr & PDE_WRITABLE) || !(attr & PTE_WRITABLE)) {
            if(first_readonly)
               *first_readonly = page;
            return false;
        }
    }

    return true;
}

uint32_t Pagedir::get_page_attr(uint32_t va, uint32_t* dir_attr) {
    unsigned dir_index = PAGE_DIRECTORY_INDEX(va);
    if(dir_attr)
        *dir_attr = _entries[dir_index];

    if(!(_entries[dir_index] & PDE_PRESENT))
        return 0;

    pagetable_t* pagetable = _tables[dir_index];
    unsigned table_index = PAGE_TABLE_INDEX(va);
    return pagetable->entries[table_index];
}

void Pagedir::set_page_attr(uint32_t va, uint32_t attr) {
    unsigned dir_index = PAGE_DIRECTORY_INDEX(va);
    assert(_entries[dir_index] & PDE_PRESENT);

    pagetable_t* pagetable = _tables[dir_index];
    unsigned table_index = PAGE_TABLE_INDEX(va);
    pagetable->entries[table_index] = attr;
}

