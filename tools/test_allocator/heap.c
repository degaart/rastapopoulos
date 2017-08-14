#include "list.h"
#include <stdint.h>
#include <assert.h>
#include <string.h>

#define ALIGN(val, align) \
    (((val) + ((align) - 1)) & ~((align) - 1))

#define TRUNCATE(val, align) \
    (((val) / (align)) * (align))

#define IS_ALIGNED(val, align) \
    (( ((uint32_t)(val)) % ((uint32_t)(align)) ) == 0)

#define MAGIC_FREEBLK       0x9891212D
#define MAGIC_BLK           0x8780191C
#define CANARY              0xAAAAAAAA
#define ELEMENT_SIZE        16

struct free_block {
    unsigned elements;
    list_declare_node(free_block) node;
    unsigned magic;
};
list_declare(free_lst, free_block);

struct allocated_block {
    unsigned elements;
    unsigned size;
    unsigned padding;
    unsigned magic;
};

struct heap {
    struct free_lst freelist;
};

void heap_grow(struct heap* heap, void* start, unsigned size);
void add_free_block(struct heap* heap, struct free_block* freeblk);

struct heap* heap_init(void* start, unsigned size)
{
    assert(sizeof(struct free_block) == ELEMENT_SIZE);
    assert(sizeof(struct allocated_block) == ELEMENT_SIZE);

    struct heap* result = (struct heap*)start;
    bzero(result, sizeof(struct heap));
    list_init(&result->freelist);
    
    heap_grow(result, (unsigned char*)result + sizeof(struct heap), size - sizeof(struct heap));

    return result;
}

void heap_grow(struct heap* heap, void* start, unsigned size)
{
    unsigned char* aligned_start = (unsigned char*)ALIGN((uintptr_t)start, ELEMENT_SIZE);
    unsigned aligned_size = 
        TRUNCATE(size - (aligned_start - (unsigned char*)start), ELEMENT_SIZE);

    assert(aligned_size > sizeof(struct free_block));

    struct free_block* hdr = (struct free_block*)aligned_start;
    bzero(hdr, sizeof(struct free_block));

    hdr->elements = (aligned_size - ELEMENT_SIZE) / ELEMENT_SIZE;
    hdr->magic = MAGIC_FREEBLK;

    add_free_block(heap, hdr);  /* keep freelist sorted */
}

unsigned heap_block_size(struct heap* heap, void* block)
{
    struct allocated_block* allocblk = (struct allocated_block*)((unsigned char*)block - sizeof(struct allocated_block));
    return allocblk->size;
}

void* heap_alloc(struct heap* heap, unsigned size)
{
    /* Number of elements to accomodate requested size */
    unsigned nelements = ((size + (ELEMENT_SIZE - 1)) / ELEMENT_SIZE) * ELEMENT_SIZE;

    /* Find best-fit block */
    struct free_block* freeblk = NULL;
    list_foreach(free_block, blk, &heap->freelist, node) {
        if(blk->elements >= nelements) {
            if(!freeblk || blk->elements < freeblk->elements) {
                freeblk = blk;
            }
        }
    }

    if(!freeblk)
        return NULL;

    /* Create new allocated_block */
    unsigned freeblk_nelements = freeblk->elements;
    list_remove(&heap->freelist, freeblk, node);

    struct allocated_block* allocblk = (struct allocated_block*)freeblk;
    allocblk->elements = freeblk_nelements;
    allocblk->size = size;
    allocblk->padding = 0;
    allocblk->magic = MAGIC_BLK;

    unsigned char* data_start = (unsigned char*)allocblk + sizeof(struct allocated_block);

    /* See if block can be split */
    if(allocblk->elements - nelements > 2) {
        struct free_block* new_freeblk;
        new_freeblk = (struct free_block*)(data_start + (allocblk->elements * ELEMENT_SIZE));
        bzero(new_freeblk, sizeof(struct free_block));
        new_freeblk->elements = nelements - allocblk->elements - 1;
        new_freeblk->magic = MAGIC_FREEBLK;
        add_free_block(heap, new_freeblk);

        allocblk->elements = nelements;
    }

    return data_start;
}

void heap_free(struct heap* heap, void* address)
{
    struct allocated_block* allocblk =
        (struct allocated_block*)((unsigned char*)address - sizeof(struct allocated_block));

    assert(allocblk->magic == MAGIC_BLK);
    unsigned elements = allocblk->elements;

    struct free_block* freeblk = (struct free_block*)allocblk;
    bzero(freeblk, sizeof(struct free_block));
    freeblk->elements = elements;
    freeblk->magic = MAGIC_FREEBLK;
    add_free_block(heap, freeblk);
}

void add_free_block(struct heap* heap, struct free_block* freeblk)
{
    /* Freelist sorted by address */
    struct free_block* prev = NULL;
    list_foreach(free_block, blk, &heap->freelist, node) {
        if(blk > freeblk)
            break;
        prev = blk;
    }

    if(!prev) {
        list_append(&heap->freelist, freeblk, node);
    } else {
        list_insert_after(&heap->freelist, freeblk, prev, node);
    }

    /* block coalescing */
    prev = list_prev(freeblk, node);
    struct free_block* next = list_next(freeblk, node);

    if(prev) {
        unsigned char* prev_end = (unsigned char*)prev + 
                                  sizeof(struct free_block) + 
                                  (prev->elements * ELEMENT_SIZE);
        if(prev_end == (unsigned char*)freeblk) {
            list_remove(&heap->freelist, freeblk, node);

            prev->elements = prev->elements + freeblk->elements + 1;
            freeblk = prev;
        }
    }

    if(next) {
        unsigned char* block_end = (unsigned char*)freeblk +
                                   sizeof(struct free_block) +
                                   (freeblk->elements * ELEMENT_SIZE);
        if(block_end == (unsigned char*)next) {
            list_remove(&heap->freelist, next, node);

            freeblk->elements = freeblk->elements + next->elements + 1;
        }
    }
}


