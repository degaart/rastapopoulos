#pragma once

#include <stdbool.h>

/* 
 * TODO: We can get rid of the next pointer, 
 * just use current address + size
 */
struct heap_block_header {
    struct heap_block_header* next;
    unsigned flags;
    unsigned size;          /* Including the header */
    unsigned magic;
};

/*
 * TODO: We can get rid of the head pointer
 * Just use address + sizeof(struct heap)
 */
struct heap {
    struct heap_block_header* head;
    unsigned size;          /* Including this header */
    unsigned max_size;
};

struct heap_info {
    void* address;
    unsigned size;
    unsigned free;
};

struct heap* heap_init(void* address, unsigned size, unsigned max_size);
struct heap_info heap_info(struct heap* heap);
struct heap_block_header* heap_grow(struct heap* heap, unsigned size);
struct heap_block_header* heap_alloc_block_aligned(struct heap* heap, unsigned size, unsigned alignment);
struct heap_block_header* heap_alloc_block(struct heap* heap, unsigned size);
void heap_free_block(struct heap* heap, struct heap_block_header* block);
bool heap_is_allocated(struct heap* heap, struct heap_block_header* block);
bool heap_is_free(struct heap* heap, struct heap_block_header* block);
void heap_dump(struct heap* heap);








