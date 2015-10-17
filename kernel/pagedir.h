#ifndef _PAGEDIR_H_
#define _PAGEDIR_H_

#include <stdint.h>

class VMM;

class Pagedir {
    static const int PTE_PRESENT        =     1;
    static const int PTE_WRITABLE       =     2;
    static const int PTE_USER           =     4;
    static const int PTE_WRITETHOUGH    =     8;
    static const int PTE_NOT_CACHEABLE  =     0x10;
    static const int PTE_ACCESSED       =     0x20;
    static const int PTE_DIRTY          =     0x40;
    static const int PTE_PAT            =     0x80;
    static const int PTE_CPU_GLOBAL     =     0x10;
    static const int PTE_LV4_GLOBAL     =     0x200;
    static const int PTE_FRAME          =     0xFFFFF000;
    static const int PTE_OFFSET         =     0x00000FFF;

    static const int PDE_PRESENT        =     1;
    static const int PDE_WRITABLE       =     2;
    static const int PDE_USER           =     4;
    static const int PDE_PWT            =     8;
    static const int PDE_PCD            =     0x10;
    static const int PDE_ACCESSED       =     0x20;
    static const int PDE_DIRTY          =     0x40;
    static const int PDE_4MB            =     0x80;
    static const int PDE_CPU_GLOBAL     =     0x100;
    static const int PDE_LV4_GLOBAL     =     0x200;
    static const int PDE_FRAME          =     0xFFFFF000;

    static const uint32_t PAGE_SIZE = 0x1000;

    friend class VMM;
    Pagedir() = delete;
    ~Pagedir();
    static Pagedir* alloc();

    struct pagetable_t {
        uint32_t entries[1024];
    };

    uint32_t _entries[1024];                    /* Entries of pagedir, with flags etc, for dumping into cr3 */
    pagetable_t* _tables[1024];                 /* Pagetables mapped in kernel-space for manipulation */
    uint32_t _physical;                         /* Physical address of this pagedir */

    void set_physical(uint32_t physical);
    uint32_t get_physical(uint32_t physical);
    uint32_t physical();                        /* Get physical address of this pagedir */
public:
    void map(uint32_t va, uint32_t pa, uint32_t flags);
    void unmap(uint32_t va);
    bool get_physical(uint32_t va, uint32_t* pa);
    bool is_mapped(uint32_t va);

    
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

    void copy_kernel_mappings(Pagedir* pagedir);            /* Copy kernel mappings from specified Pagedir */
};

#endif
