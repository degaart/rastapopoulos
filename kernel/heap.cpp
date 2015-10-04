#include "heap.h"
#include "debug.h"
#include "util.h"
#include "string.h"
#include "kmalloc.h"

Heap::Heap()
: _head(nullptr), _size(0) {
    
}

void Heap::init(void* base, unsigned size) {
    TRACE("sizeof(block_t) == %zu", sizeof(Block));
    _head = Block::create(base, size);
    _head->set_last(true);
    
    _size = size;
}

void* Heap::alloc_impl(unsigned size, unsigned alignment) {
    if(!_head)
        PANIC("Uninitialized Heap");

    if(alignment <= 1)
        alignment = 1;
    
    Block* block = _head;
    while (block) {
        if(!block->used()) {
            uint8_t* aligned_start = align( block->data(), alignment );
            unsigned offset = aligned_start - (uint8_t*)block - sizeof(Block);
            if(offset != 0) {
                while ( (offset < sizeof(Block)) && (offset < block->size - sizeof(Block)) ) {
                    offset += alignment;
                }
            }
            
            if(offset + size < block->size) {
                Block* new_block;
                
                /* split for alignment */
                if(offset != 0) {
                    new_block = block->split(offset);
                } else {
                    new_block = block;
                }
                
                if(new_block) {
                    /* Split remaining free space into new block (if applicable) */
                    unsigned split_offset = sizeof(Block) + size;
                    unsigned free_space_size = new_block->size - split_offset;
                    
                    if(free_space_size > sizeof(Block)) {
                        new_block->split(split_offset);
                    }

                    new_block->set_used(true);
                    
                    void* ptr = new_block->data();
                    memset(ptr, 0x69, size);
                    
                    return ptr;
                }
            }
        }

        block = block->next();
    }
    
    return nullptr;
}

void Heap::free(void* ptr) {
    Block* block = Block::at((uint8_t*)ptr - sizeof(Block));
    if(!block->used())
        PANIC("Double-free detected for %p", ptr);
    
    block->set_used(false);
    for(Block* prev = _head; prev && prev < block; prev = prev->next()) {
        if(prev->next() == block) {
            if(!prev->used()) {
                prev->merge();
                block = prev;
                break;
            }
        }
    }

    Block* next = block->next();
    if(next && !next->used()) {
        block->merge();
    }
}

void Heap::dump() {
    TRACE("------------ HEAP DUMP ------------");
    for(Block* block = _head; block; block = block->next()) {
        uint32_t u32block = (uint32_t)block;
        TRACE("\t0x%X - 0x%X (%d) : %s %s",
              u32block, u32block + block->size - 1, block->size,
              block->used() ? "used" : "free",
              block->last() ? "last" : ""
        );
    }
    TRACE("Total: %u bytes, free: %u bytes", _size, free_size());
    TRACE("---------- EOF HEAP DUMP ----------");
}

void Heap::test_split() {
    TRACE("Testing Heap::split");
    Heap heap;
    heap.init(kmalloc(4096), 4096);
    
    Block* block = heap._head;
    Block* new_block = block->split(1024);
    
    assert(block->size == 1024);
    assert((uint8_t*)new_block == ((uint8_t*)block) + 1024);
    assert(new_block->size == 4096 - 1024);
    assert(block->size + new_block->size == 4096);
    heap.dump();
    assert(block->next() == new_block);
    
    new_block = block->split(sizeof(Block));
    heap.dump();
    assert(new_block == nullptr);
    assert(block->size == 1024);
}

void Heap::test_alloc() {
    Heap heap;
    heap.init(kmalloc(4096), 4096);
    heap.dump();

    TRACE("Testing simple aligned allocs");
    uint8_t* p0 = heap.alloc<uint8_t>(64, 4);
    heap.dump();
    assert(p0 != nullptr);
    assert(  (uint32_t)p0 % 4 == 0);
    
    uint8_t* p1 = heap.alloc<uint8_t>(64, 4);
    heap.dump();
    assert(p1 != nullptr);
    assert( (uint32_t)p1 % 4 == 0 );
    
    uint8_t* p2 = heap.alloc<uint8_t>(64, 64);
    heap.dump();
    assert(p2 != nullptr);
    assert( (uint32_t)p2 % 64 == 0);
    
    uint8_t* p3 = heap.alloc<uint8_t>(64, 64);
    heap.dump();
    assert(p3 != nullptr);
    assert( (uint32_t)p3 % 64 == 0);
    
    uint8_t* p4 = heap.alloc<uint8_t>(2, 2);
    heap.dump();
    assert(p4 != nullptr);
    assert( (uint32_t)p4 % 2 == 0);
    
    uint8_t* p5  = heap.alloc<uint8_t>(20);
    heap.dump();
    assert(p5 != nullptr);
    
    heap.free(p5);
    heap.dump();
    
    heap.free(p4);
    heap.dump();
    
    heap.free(p3);
    heap.dump();
    
    heap.free(p2);
    heap.dump();
    
    heap.free(p1);
    heap.dump();
    
    
    heap.free(p0);
    heap.dump();    
}

void Heap::grow(unsigned size) {
    /* find end of heap */
    Block* last_block = _head;
    while(!last_block->last())
        last_block = last_block->next();
    assert(last_block != nullptr);

    if(last_block->used()) {
        /* create new block after last_block */
        Block* new_block = Block::create((uint8_t*)last_block + last_block->size, size);
        last_block->set_last(false);
        new_block->set_last(true);

        _size += size;
    } else {
        /* grow block */
        last_block->size += size;
        _size += size;
    }
}

void Heap::check() {
    for(Block* b = _head; b; b = b->next())
        ;
}

unsigned Heap::free_size() {
    unsigned size = 0;
    for(Block* b = _head; b; b = b->next()) {
        if(!b->used()) {
            size += b->size - sizeof(Block);
        }
    }
    return size;
}

