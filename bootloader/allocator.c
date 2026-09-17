#include "allocator.h"
#include "format.h"

extern void halt(void);

struct FreeBlock
{
    size_t size; /* Entire block, including this field. */
    struct FreeBlock* next;
};

static struct FreeBlock* free_list;

void heap_init(void* arena, size_t size)
{
    unsigned char* p = arena;

    if (!size) {
        free_list = 0;
        return;
    }

    /* Word-align the arena */
    if ((size_t)p & 1) {
        ++p;
        --size;
    }
    size &= (size_t)~1;

    if (size < sizeof(struct FreeBlock)) {
        free_list = 0;
        return;
    }

    free_list = (struct FreeBlock*)p;
    free_list->size = size;
    free_list->next = 0;
}

void* malloc(size_t size)
{
    struct FreeBlock** link = &free_list;
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
    printf("Out of memory\n");
    halt();
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
    next = free_list;
    while (next && next < block) {
        prev = next;
        next = next->next;
    }

    block->next = next;
    if (prev)
        prev->next = block;
    else
        free_list = block;

    if (next && (unsigned char*)block + block->size == (unsigned char*)next) {
        block->size += next->size;
        block->next = next->next;
    }

    if (prev && (unsigned char*)prev + prev->size == (unsigned char*)block) {
        prev->size += block->size;
        prev->next = block->next;
    }
}
