#include "../debug.h"
#include "../util.h"
#include "../vmm.h"
#include "../pmm.h"
#include "../string.h"

#define MAGIC_ALLOCATED             0x01234567
#define MAGIC_FREE                  0x76543210 
#define BLOCK_ALLOCATED             0x1

struct header {
    struct header* next;
    unsigned flags;
    unsigned size;          /* Including the header */
    uint32_t magic;
};

struct heap_info {
    void* address;
    unsigned size;
    unsigned free;
};

/*
 * Stores free and allocated blocks
 * Should be kept sorted by address
 */
static struct header* kernel_heap = NULL;

static bool is_valid_block(struct header* header)
{
    return header->magic == MAGIC_FREE || 
           header->magic == MAGIC_ALLOCATED;
}

static struct header* last_block()
{
    struct header* h;
    for(h = kernel_heap; h && h->next; h = h->next)
        assert(is_valid_block(h));
    return h;
}

static struct header* prev_block(struct header* block)
{
    assert(is_valid_block(block));

    if(block == kernel_heap)
        return NULL;

    struct header* prevh;
    for(prevh = kernel_heap; prevh && prevh->next != block; prevh = prevh->next)
        assert(is_valid_block(prevh));
    return prevh;
}

static bool is_free(struct header* block)
{
    assert(is_valid_block(block));

    bool result = (block->flags & BLOCK_ALLOCATED) == 0 &&
                  block->magic == MAGIC_FREE;
    return result;
}

static bool is_allocated(struct header* block)
{

    assert(is_valid_block(block));
    bool result = (block->flags & BLOCK_ALLOCATED) != 0 &&
                  block->magic == MAGIC_ALLOCATED;
    return result;
}

static void set_allocated(struct header* block)
{
    assert(is_valid_block(block));

    block->flags |= BLOCK_ALLOCATED;
    block->magic = MAGIC_ALLOCATED;
}

static void set_free(struct header* block)
{
    assert(is_valid_block(block));

    block->flags &= ~BLOCK_ALLOCATED;
    block->magic = MAGIC_FREE;
}

static struct header* create_block(void* addr, unsigned size)
{
    struct header* result = addr;
    memset(result, 0, sizeof(struct header));
    result->size = size;
    result->magic = MAGIC_FREE;
    return result;
}

static struct header* merge_blocks(struct header* dest, struct header* source)
{
    assert(dest < source);

    dest->size += source->size;
    dest->next = source->next;

    memset(source, 0, sizeof(struct header));

    return dest;
}

void kernel_heap_init(void* address, unsigned size)
{
    assert(IS_ALIGNED(size, PAGE_SIZE));
        
    /* We start by putting the initial heap just after the kernel */
    trace("Initializing kernel heap at %p, size %d", address, size);
    kernel_heap = create_block(address, size);
}

struct heap_info heap_info()
{
    struct heap_info result = {0};
    result.address = kernel_heap;
    for(struct header* h = kernel_heap; h; h = h->next) {
        result.size += h->size;
        if(is_free(h))
            result.free += h->size - sizeof(struct header);
    }
    return result;
}

struct header* grow_heap(unsigned size)
{
    size = ALIGN(size, PAGE_SIZE);
    trace("Growing heap by %d", size);

    struct header* last = last_block();
    assert(last != NULL);

    unsigned char* end_of_heap = ((unsigned char*)last) + last->size;
    if(vmm_paging_enabled()) {
        for(unsigned char* page = end_of_heap; page < end_of_heap + size; page += PAGE_SIZE) {
            uint32_t frame = pmm_alloc();
            assert(frame != PMM_INVALID_PAGE);

            vmm_map((uint32_t)page, frame, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
        }
    }

    struct header* new_header = create_block(end_of_heap, size);
    last->next = new_header;

    if(is_free(last)) {
        new_header = merge_blocks(last, new_header);
    }

    return new_header;
}

struct header* alloc_block(unsigned size)
{
    while(true) {
        /* Walk blocklist to find fitting block */
        struct header* prevh = NULL;
        for(struct header* h = kernel_heap; h; prevh = h, h = h->next) {
            if(is_free(h) && (h->size - sizeof(struct header) >= size)) {
                // trace("Testing block %p", h);
                if(h->size - sizeof(struct header) == size) {
                    set_allocated(h);
                    return h;
                } else if(h->size - sizeof(struct header) > size + sizeof(struct header)) {
                    unsigned old_size = h->size;

                    /* Split into two blocks */
                    unsigned char* new_block_address = ((unsigned char*)h) + size + sizeof(struct header);
                    unsigned new_block_size = h->size - sizeof(struct header) - size;
                    struct header* new_block = create_block(new_block_address, new_block_size);
                    new_block->next = h->next;

                    h->size = sizeof(struct header) + size;
                    h->next = new_block;
                    set_allocated(h);

                    assert(new_block->size + h->size == old_size);
                    return h;
                } else {
                    /* Return this block without modifying it's size */
                    set_allocated(h);
                    return h;
                }
            }
        }

        /* No fitting block found, add more memory to heap */
        grow_heap(size);
    }
}

void free_block(struct header* block)
{
    assert(is_valid_block(block));
    assert(is_allocated(block));
    set_free(block);

    /* If previous block free, coalesce with previous block */
    struct header* prevh = prev_block(block);
    if(prevh && is_free(prevh)) {
        block = merge_blocks(prevh, block);
    }

    if(block->next && is_free(block->next)) {
        block = merge_blocks(block, block->next);
    }
}

static void dump_heap()
{
    trace("Heap dump:");
    for(struct header* h = kernel_heap; h; h = h->next) {
        trace("\t%p: size: %d, allocated: %s, next: %p",
              h,
              h->size,
              is_allocated(h) ? "true" : "false",
              h->next);
    }

}

void test_kmalloc()
{
    trace("Testing kmalloc()");

    /* Create heap at 48Mb */
    unsigned char* heap_start = (unsigned char*)0x3000000;
    vmm_map((uint32_t)heap_start, pmm_alloc(), VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);

    kernel_heap_init(heap_start, PAGE_SIZE);
    assert(is_free(kernel_heap));

    assert(kernel_heap->next == NULL);

    /* Allocate 4 blocks of 4 bytes */
    struct header* blocks[4];
    for(int i = 0; i < sizeof(blocks) / sizeof(blocks[4]); i++) {
        trace("Allocating block %d", i);
        blocks[i] = alloc_block(sizeof(uint32_t));
        assert(is_allocated(blocks[i]));
    }

    /* Free those 4 blocks */
    trace("Freeing block 0");
    free_block(blocks[0]);
    assert(is_allocated(blocks[1]));
    assert(is_allocated(blocks[2]));
    assert(is_allocated(blocks[3]));

    trace("Freeing block 1");
    free_block(blocks[1]);
    assert(is_allocated(blocks[2]));
    assert(is_allocated(blocks[3]));

    trace("Freeing block 2");
    free_block(blocks[2]);
    assert(is_allocated(blocks[3]));

    trace("Freeing block 3");
    free_block(blocks[3]);

    /* Normally, if we allocate again, we should have the same addresses as result */
    struct header* new_blocks[4];
    for(int i = 0; i < sizeof(new_blocks) / sizeof(new_blocks[4]); i++) {
        trace("Allocating block %d again", i);
        new_blocks[i] = alloc_block(sizeof(uint32_t));
        assert(is_allocated(blocks[i]));
    }

    assert(blocks[0] == new_blocks[0]);
    assert(blocks[1] == new_blocks[1]);
    assert(blocks[2] == new_blocks[2]);
    assert(blocks[3] == new_blocks[3]);

    /*
     * free blocks 1 & 3, and then 2. 
     * If we then allocate a new block of size > sizeof(uint32_t)
     * we should have the same 
     * address as block[1] because of coalescing
     */
    free_block(blocks[1]);
    free_block(blocks[3]);
    free_block(blocks[2]);
    
    new_blocks[1] = alloc_block(sizeof(uint32_t) * 4);
    assert(new_blocks[1] == blocks[1]);

    /* Free all these allocated blocks so we have a clean heap again */
    free_block(blocks[0]);
    free_block(new_blocks[1]);
    assert(kernel_heap->size == PAGE_SIZE);

    /* Test heap growing code by allocating 4096 * 2 bytes */
    struct header* big_block = alloc_block(PAGE_SIZE * 2);
    assert(big_block != NULL);
    assert(big_block->size == sizeof(struct header) + (PAGE_SIZE * 2));
    
    struct heap_info hi = heap_info();
    assert(hi.size == PAGE_SIZE * 3);

    /* Which means we can basically allocate (4096 * 3) - (sizeof(struct header) * 2) - (4096 * 2)
     * whithout growing the heap
     */
    unsigned alloc_size = (PAGE_SIZE * 3) - (sizeof(struct header) * 2) - (PAGE_SIZE * 2);
    struct header* small_block = alloc_block(alloc_size);

    hi = heap_info();
    assert(hi.size == PAGE_SIZE * 3);

    /* Free them to clean up heap */
    free_block(small_block);
    free_block(big_block);

    /* 
     * Aaaaand we can then alloc (4096 * 3) - sizeof(struct header)
     * without growing the heap
     */
    alloc_size = (PAGE_SIZE * 3) - sizeof(struct header);
    big_block = alloc_block(alloc_size);

    hi = heap_info();
    assert(hi.size == PAGE_SIZE * 3);

    free_block(big_block);
}




