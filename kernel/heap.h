#ifndef __kdev__heap__
#define __kdev__heap__

#include <stdint.h>
#include "linked_list.h"

/*
    EDIT: *** Some of this information is incomplete ***
 
    kmalloc manages kernel-space memory. We assume kernel-space memory
    is restricted to VA 0x00000000 - 0x08000000
 
    kmalloc keeps track of memory using units called "Blocks", using an
    ordered list.
 
    A Block:
        - can be "used" or "unused"
        - has a "base" and a "size"
        - we don't care abouts it's location in physical memory, as we only
            manage kernel-space memory.
 
    Initially, we allocate a single free block, before paging is enabled,
    so we can allocate page frames for VMM::map for kmalloc to use.
    
    On allocation:
        - find smallest unused block that satisfies user request. Shrink block.
            Create new block the size of requested memory + header. Mark as allocated.
            Return new block
    On free:
        - Mark block as free
        - Check adjacent blocks, check if they are free. If they are free, merge them with
            present block
 */

/* Manages blocks of VA space. PA management and mapping by kmalloc */
class Heap {
private:
    class Block {
    private:
        uint8_t flags;
        uint32_t magic;
        
        static const int MAGIC = 0x12345678;
        static const int MAGIC_DESTROYED = 0x87654321;
        static const int USED = 0x1;
        static const int LAST = 0x2;
    public:
        uint32_t size;
        
        static Block* at(void* addr);                               /* get block at specified address */
        static Block* create(void* addr, uint32_t size);            /* Create new (free & !last) block as specified address */
        
        void check();
        bool used();
        void set_used(bool used);
        bool last();
        void set_last(bool last);
        void destroy();
        void merge();   /* Merge with next block, and destroy the latter */

        /*
            Split at the specified offset
            Offset is exprimed relative to start of Block (i.e. address of block)
            Returns new block if successful Returns NULL if not enough space to split block
         */
        Block* split(uint32_t offset);
        
        uint8_t* data();
        Block* next();
    private:
        Block();
        ~Block();
    } __attribute__((packed));
    
    Block* _head;
    unsigned _size;         /* total size of this heap (not including header overhead) */
    unsigned _free;         /* free bytes in heap (including header overhead) */
    
    void* alloc_impl(unsigned size, unsigned alignment);
public:
    Heap();
    void init(void* base, unsigned size);
    void grow(unsigned size);                       /* Increase heap size by specified number of bytes */
    
    template<typename T>
    T* alloc(unsigned size, unsigned alignment = 0) { /* Returns 0 if cannot allocate mem */
        return( static_cast<T*>(alloc_impl(size, alignment)) );
    }
    
    void free(void*);
    void dump();
    
    unsigned total_size() {
        return _size;
    }
    
    unsigned free_size() {
        return _free;
    }
    
    uint8_t* head() {
        return (uint8_t*)_head;
    }
    
    static void test_split();
    static void test_alloc();
};

#endif /* defined(__kdev__heap__) */



