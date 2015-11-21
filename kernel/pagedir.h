#ifndef _PAGEDIR_H_
#define _PAGEDIR_H_

#include <stdint.h>
#include <stddef.h>

class VMM;

class Pagedir {
    static const int PTE_PRESENT        =   (1);
    static const int PTE_WRITABLE       =   (1 << 1);
    static const int PTE_USER           =   (1 << 2);
    static const int PTE_WRITETHOUGH    =   (1 << 3);
    static const int PTE_NOT_CACHEABLE  =   (1 << 4);
    static const int PTE_ACCESSED       =   (1 << 5);
    static const int PTE_DIRTY          =   (1 << 6);
    static const int PTE_PAT            =   (1 << 7);
    static const int PTE_CPU_GLOBAL     =   (1 << 8);
    static const int PTE_AVL0           =   (1 << 9);
    static const int PTE_AVL1           =   (1 << 10);
    static const int PTE_ALLOCATED      =   (1 << 11);          /* pageframe should be freed on deallocation */
    static const int PTE_FRAME          =   0xFFFFF000;
    static const int PTE_OFFSET         =   0x00000FFF;

    static const int PDE_PRESENT        =     (1);
    static const int PDE_WRITABLE       =     (1 << 1);
    static const int PDE_USER           =     (1 << 2);
    static const int PDE_PWT            =     (1 << 3);
    static const int PDE_PCD            =     (1 << 4);
    static const int PDE_ACCESSED       =     (1 << 5);
    static const int PDE_AVL0           =     (1 << 6); /* Used only if 4mb page */
    static const int PDE_AVL1           =     (1 << 7); /* Used only if 4mb page */
    static const int PDE_AVL2           =     (1 << 8);
    static const int PDE_AVL3           =     (1 << 9);
    static const int PDE_AVL4           =     (1 << 10);
    static const int PDE_AVL5           =     (1 << 11);
    static const int PDE_FRAME          =     0xFFFFF000;

    static const uint32_t PAGE_SIZE = 0x1000;

    friend class VMM;
    Pagedir() = delete;                         /* Because must be aligned on a page boundary */
    ~Pagedir();
    static Pagedir* create();

    struct pagetable_t {
        uint32_t entries[1024];
    };

    uint32_t _entries[1024];                    /* Entries of pagedir, with flags etc, for dumping into cr3 */
    pagetable_t* _tables[1024];                 /* Pagetables mapped in kernel-space for manipulation */
    uint32_t _physical;                         /* Physical address of this pagedir */

    void set_physical(uint32_t physical);
    uint32_t get_physical(uint32_t physical);
    void set_page_attr(uint32_t va, uint32_t attr); /* This function is dangerous!, be careful when calling it! */
public:
    void map(uint32_t va, uint32_t pa, uint32_t flags);
    void unmap(uint32_t va);
    bool get_physical(uint32_t va, uint32_t* pa);
    bool is_mapped(uint32_t va);
    uint32_t physical();                                        /* Get physical address of this pagedir */
    uint32_t get_page_attr(uint32_t va, uint32_t* dir_attr);
    uint32_t alloc(uint32_t va, uint32_t flags);
    void dealloc(uint32_t va);
    bool check_readable_block(const void* va, size_t size, uint32_t* first_unreadable, uint32_t* first_invalid);
    bool check_writable_block(const void* va, size_t size, uint32_t* first_readonly, uint32_t* first_invalid);
    bool allocated(void* va);   /* was page allocated with Pagedir::alloc() */

    void map(void* va, uint32_t pa, uint32_t flags) {
        map((uint32_t)va, pa, flags);
    }

    void unmap(void* va) {
        unmap((uint32_t)va);
    }

    bool get_physical(void* va, uint32_t* pa) {
        return get_physical((uint32_t)va, pa);
    }

    bool is_mapped(void* va) {
        return is_mapped((uint32_t)va);
    }

    uint32_t alloc(void* va, uint32_t flags) {
        return alloc((uint32_t)va, flags);
    }

    void dealloc(void* va) {
        dealloc((uint32_t)va);
    }

    void copy_kernel_mappings(Pagedir* pagedir);            /* Copy kernel mappings from specified Pagedir */
};

#endif
