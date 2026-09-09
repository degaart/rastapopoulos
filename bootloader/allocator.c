#include "allocator.h"

struct FreeBlock
{
    size_t size; /* Entire block, including this field. */
    struct FreeBlock* next;
};

static struct FreeBlock* freeList;

void heapInit(void* arena, size_t size)
{
    unsigned char* p = arena;

    if (!size) {
        freeList = 0;
        return;
    }

    /* Word-align the arena */
    if ((size_t)p & 1) {
        ++p;
        --size;
    }
    size &= (size_t)~1;

    if (size < sizeof(struct FreeBlock)) {
        freeList = 0;
        return;
    }

    freeList = (struct FreeBlock*)p;
    freeList->size = size;
    freeList->next = 0;
}

void* malloc(size_t size)
{
    struct FreeBlock** link = &freeList;
    struct FreeBlock* block;
    struct FreeBlock* rest;
    size_t needed;

    if (!size || size > (size_t)-1 - sizeof(size_t) - 1)
        return 0;

    needed = (size + sizeof(size_t) + 1) & (size_t)~1;

    while ((block = *link) != 0) {
        if (block->size >= needed) {
            if (block->size - needed >= (size_t)sizeof(struct FreeBlock)) {
                rest = (struct FreeBlock*)((unsigned char*)block + needed);
                rest->size = block->size - needed;
                rest->next = block->next;
                *link = rest;
                block->size = needed;
            } else
                *link = block->next;

            return (unsigned char*)block + sizeof(size_t);
        }
        link = &block->next;
    }
    return 0;
}

void free(void* ptr)
{
    struct FreeBlock* block;
    struct FreeBlock* prev = 0;
    struct FreeBlock* next;

    if (!ptr)
        return;

    block = (struct FreeBlock*)((unsigned char*)ptr - sizeof(size_t));

    /* Keep the list address-sorted so neighboring blocks can be merged */
    next = freeList;
    while (next && next < block) {
        prev = next;
        next = next->next;
    }

    block->next = next;
    if (prev)
        prev->next = block;
    else
        freeList = block;

    if (next && (unsigned char*)block + block->size == (unsigned char*)next) {
        block->size += next->size;
        block->next = next->next;
    }

    if (prev && (unsigned char*)prev + prev->size == (unsigned char*)block) {
        prev->size += block->size;
        prev->next = block->next;
    }
}
