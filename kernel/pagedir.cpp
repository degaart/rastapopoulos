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
    for(unsigned i = 0; i < 1024; i++)
        kfree(_tables[i]);
}

Pagedir* Pagedir::alloc() {
    Pagedir* dir = (Pagedir*)kmalloc_a(sizeof(Pagedir), PAGE_SIZE);
    bzero(dir->_entries, sizeof(dir->_entries));
    bzero(dir->_tables, sizeof(dir->_tables));

    bool got_pysical = VMM::get_physical(dir, &dir->_physical);
    assert(got_pysical);
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

        TRACE("Allocated new pagetable: %p (physical 0x%X)", page_table, table_physical);
        bzero(page_table, sizeof(pagetable_t));

        _entries[dir_index] = (table_physical & PDE_FRAME) | PDE_PRESENT | PDE_WRITABLE | PDE_USER; /* TODO: Remove PDE_USER for kernel code & heap */
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
    page_table->entries[table_index] = pa | flags | PTE_USER; /* TODO: Remove PTE_USER for kernel code & heap */
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
    }

    uint32_t pa = get_physical(va);
    page_table->entries[table_index] &= ~PTE_PRESENT;
    PMM::free(pa);
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

    /* TODO: Handle remaining maps > 3Gb */
}

uint32_t Pagedir::physical() {
    return _physical;
}

