#pragma once

#include <stdbool.h>
#include <stdint.h>

/* 
 * TODO: We can get rid of the next pointer, 
 * just use current address + size
 */
struct heap_block_header {
    unsigned checksum;
    unsigned flags;
    unsigned size;          /* Including the header */
    struct heap_block_header* next;
};

/*
 * TODO: We can get rid of the head pointer
 * Just use address + sizeof(struct heap)
 */
struct heap {
    struct heap_block_header* head;
    unsigned size;          /* Including this header */
    unsigned max_size;
    volatile uint32_t lock;
};

struct heap_info {
    void* address;
    unsigned size;
    unsigned free;
};

struct heap* heap_init(void* address, unsigned size, unsigned max_size);
struct heap_info heap_info(struct heap* heap);
struct heap_block_header* heap_alloc_block_aligned(struct heap* heap, unsigned size, unsigned alignment);
struct heap_block_header* heap_alloc_block(struct heap* heap, unsigned size);
void heap_free_block(struct heap* heap, struct heap_block_header* block);
void heap_dump(struct heap* heap);
bool heap_lock(struct heap* heap);
bool heap_unlock(struct heap* heap);









