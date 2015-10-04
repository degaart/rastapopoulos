#include "heap.h"

Heap::Block* Heap::Block::at(void* addr) {
    Block* block = (Block*)addr;
    block->check();
    return block;
}

Heap::Block* Heap::Block::create(void* addr, uint32_t size) {
    Block* block = (Block*)addr;
    block->magic = MAGIC;
    block->size = size;
    block->flags = 0;
    return block;
}

void Heap::Block::check() {
    if(magic != MAGIC)
        PANIC("Invalid block detected at %p", this);
}

bool Heap::Block::used() {
    check();
    return flags & USED;
}

void Heap::Block::set_used(bool used) {
    check();
    if(used)
        flags |= USED;
    else
        flags &= ~USED;
}

bool Heap::Block::last() {
    check();
    return flags & LAST;
}

void Heap::Block::set_last(bool last) {
    check();
    if(last)
        flags |= LAST;
    else
        flags &= ~LAST;
}

void Heap::Block::destroy() {
    magic = MAGIC_DESTROYED;
}

uint8_t* Heap::Block::data() {
    check();
    return ((uint8_t*)this) + sizeof(Heap::Block);
}

Heap::Block* Heap::Block::next() {
    check();
    if(last())
        return nullptr;
    return Block::at( ((uint8_t*)this) + size );
}

void Heap::Block::merge() {
    check();
    Block* next_block = next();
    
    if(used() != next_block->used())
        PANIC("Trying to merge used and free block at %p and %p", this, next_block);
    
    size += next_block->size;
    set_last(next_block->last());
    next_block->destroy();
}

//Heap::Block* Heap::Block::split(Heap::Block** block, Heap::Block* prev, uint32_t offset) {
//    assert(offset < (*block)->size);
//    
//    if(prev && (prev->used() == (*block)->used())) {
//        Block* new_block = create( (uint8_t*)(*block) + offset, (*block)->size - offset);
//        new_block->set_last((*block)->last());
//        
//        prev->size += offset;
//        prev->set_last(false);
//        (*block)->destroy();
//        *block = prev;
//
//        return new_block;
//    } else {
//        if(offset <= sizeof(Block))
//            return nullptr;
//        
//        Block* new_block = create( (uint8_t*)(*block) + offset, (*block)->size - offset );
//        new_block->set_last((*block)->last());
//        (*block)->size = offset;
//        (*block)->set_last(false);
//        return new_block;
//    }
//    return nullptr;
//}

Heap::Block* Heap::Block::split(uint32_t offset) {
    assert(offset < size);
    if(offset <= sizeof(Block))
        return nullptr;
    
    Block* new_block = create( (uint8_t*)this + offset, size - offset );
    new_block->flags = flags;
    new_block->set_last(last());
    size = offset;
    set_last(false);
    return new_block;
}



