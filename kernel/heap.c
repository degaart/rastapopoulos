#include "heap.h"
#include "util.h"
#include "debug.h"
#include "string.h"
#include "pmm.h"
#include "vmm.h"
#include "locks.h"

#define MAGIC_ALLOCATED             0xABCDEF01
#define MAGIC_FREE                  0x12345678
#define CANARY                      0x7778798081828384

#define BLOCK_ALLOCATED             0x1

static void dump_heap(struct heap* heap);

static bool is_valid_block(struct heap* heap, struct heap_block_header* header)
{
    if(header->flags & BLOCK_ALLOCATED)
        return header->magic == MAGIC_ALLOCATED;
    else
        return header->magic == MAGIC_FREE;
}

static struct heap_block_header* last_block(struct heap* heap)
{
    struct heap_block_header* h;
    for(h = heap->head; h && h->next; h = h->next)
        assert(is_valid_block(heap, h));
    return h;
}

static struct heap_block_header* prev_block(struct heap* heap, struct heap_block_header* block)
{
    assert(is_valid_block(heap, block));

    if(block == heap->head)
        return NULL;

    struct heap_block_header* prevh;
    for(prevh = heap->head; prevh && prevh->next != block; prevh = prevh->next)
        assert(is_valid_block(heap, prevh));
    return prevh;
}

void heap_record_dump();

bool heap_is_free(struct heap* heap, struct heap_block_header* block)
{
    if(!is_valid_block(heap, block)) {
        panic("Invalid block detected at %p (buffer: %p, magic: %p)", 
              block,
              ((unsigned char*)block) + sizeof(struct heap_block_header),
              block->magic);
    }

    bool result = (block->flags & BLOCK_ALLOCATED) == 0;
    return result;
}

bool heap_is_allocated(struct heap* heap, struct heap_block_header* block)
{
    assert(is_valid_block(heap, block));
    bool result = (block->flags & BLOCK_ALLOCATED) != 0;
    return result;
}

static void set_allocated(struct heap* heap, struct heap_block_header* block)
{
    assert(is_valid_block(heap, block));

    block->flags |= BLOCK_ALLOCATED;
    block->magic = MAGIC_ALLOCATED;
}

static void set_free(struct heap* heap, struct heap_block_header* block)
{
    assert(is_valid_block(heap, block));

    block->flags &= ~BLOCK_ALLOCATED;
    block->magic = MAGIC_FREE;
}

static void write_canary(struct heap_block_header* block)
{
    uint64_t* canary = (uint64_t*)(((unsigned char*)block) + block->size - sizeof(uint64_t));
    *canary = CANARY;
}

static void check_canary(struct heap_block_header* block)
{
    uint64_t* canary = (uint64_t*)(((unsigned char*)block) + block->size - sizeof(uint64_t));
    if(*canary != CANARY) {
        panic("Buffer overrun detected for block %p", (unsigned char*)block + sizeof(struct heap_block_header));
    }
}

static struct heap_block_header* create_block(struct heap* heap, void* addr, unsigned size)
{
    struct heap_block_header* result = addr;
    memset(result, 0, sizeof(struct heap_block_header));
    result->size = size;
    result->magic = MAGIC_FREE;
    return result;
}

static struct heap_block_header* merge_blocks(struct heap* heap, struct heap_block_header* dest, struct heap_block_header* source)
{
    assert(dest < source);
    assert(dest->magic == source->magic);

    dest->size += source->size;
    dest->next = source->next;

    return dest;
}

struct heap* heap_init(void* address, unsigned size, unsigned max_size)
{
    assert(IS_ALIGNED((uint32_t)address, PAGE_SIZE));
    assert(IS_ALIGNED(size, PAGE_SIZE));
        
    /* We start by putting the initial heap just after the kernel */
    trace("Initializing heap at %p, size %d, max %d", address, size, max_size);
    struct heap* result = (struct heap*)address;
    memset(result, 0, sizeof(struct heap));
    result->size = size;
    result->max_size = max_size;
    result->lock = SPINLOCK_INIT;
    assert(IS_ALIGNED(&result->lock, sizeof(uint32_t)));

    unsigned char* head = (unsigned char*)address + sizeof(struct heap);
    unsigned head_size = size - sizeof(struct heap);
    result->head = create_block(result, head, head_size);

    return result;
}

struct heap_info heap_info(struct heap* heap)
{
    struct heap_info result = {0};
    result.address = heap;
    result.size = heap->size;
    for(struct heap_block_header* h = heap->head; h; h = h->next) {
        if(heap_is_free(heap, h))
            result.free += h->size - sizeof(struct heap_block_header);
    }
    return result;
}

static struct heap_block_header* heap_grow(struct heap* heap, unsigned size)
{
    size = ALIGN(size, PAGE_SIZE);
    trace("Growing heap by %d", size);

    struct heap_block_header* last = last_block(heap);
    assert(last != NULL);

    unsigned allocated = 0;
    unsigned char* end_of_heap = ((unsigned char*)last) + last->size;
    assert(IS_ALIGNED((uint32_t)end_of_heap, PAGE_SIZE));

    for(unsigned char* page = end_of_heap; page < end_of_heap + size; page += PAGE_SIZE) {
        if(heap->size >= heap->max_size)
            break;

        if(vmm_paging_enabled()) {
            uint32_t frame = pmm_alloc();
            if(frame == PMM_INVALID_PAGE)
                break;

            vmm_map(page, frame, VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);
        } else if(pmm_initialized()) {
            pmm_reserve((uint32_t)page);
        }

        heap->size += PAGE_SIZE;
        allocated += PAGE_SIZE;
    }

    if(allocated) {
        //trace("Heap grown by %d bytes", allocated);

        struct heap_block_header* new_header = create_block(heap, end_of_heap, allocated);
        last->next = new_header;

        if(heap_is_free(heap, last)) {
            new_header = merge_blocks(heap, last, new_header);
        }

        return new_header;
    } else {
        return NULL;
    }
}

struct heap_block_header* heap_alloc_block_aligned(struct heap* heap, unsigned size, unsigned alignment)
{
    bool locked = heap_lock(heap);
    if(!locked) {
        panic("Concurrent heap modification detected");
    }

    unsigned data_size = size;
    size += sizeof(uint64_t);   /* for canary at end of allocated block */

    while(true) {
        /* Walk blocklist to find fitting block */
        struct heap_block_header* prevh = NULL;
        for(struct heap_block_header* h = heap->head; h; prevh = h, h = h->next) {
            if(heap_is_free(heap, h) && (h->size - sizeof(struct heap_block_header) >= size)) {
                /*
                 * Memory layout
                 * ----------
                 * |header0 | --> h
                 * ----------
                 * |data0   | --> size: header1 - header0 - sizeof(struct heap_block_header)
                 * ----------
                 * |header1 | --> data1 - sizeof(struct heap_block_header)
                 * ----------
                 * |data1   | --> ALIGN(header0 + sizeof(struct heap_block_header))
                 * ----------
                 * |header2 | --> data1 + data1.size
                 * ----------
                 * |data2   | --> size: h.size - 
                 * |        |           sizeof(struct heap_block_header) - data0.size -
                 * |        |           sizeof(struct heap_block_header) - data1.size -
                 * |        |           sizeof(struct heap_block_header)
                 * ---------- --> header0 + h.size
                 */
                unsigned char* header0 = (unsigned char*)h;
                unsigned char* data1 = (unsigned char*)ALIGN((uint32_t)header0 + sizeof(struct heap_block_header), alignment);
                if(data1 == header0 + sizeof(struct heap_block_header)) {                              /* Aligned size corresponds to data start */
                    if(h->size >= sizeof(struct heap_block_header) + size) {                           /* Size of block is enough for header and data */
                        unsigned remaining = h->size - sizeof(struct heap_block_header) - size;
                        if(remaining > sizeof(struct heap_block_header)) {                             /* The remaining bytes can be used as a new block */
                            unsigned old_size = h->size;

                            unsigned char* new_block_address = header0 + size + sizeof(struct heap_block_header);
                            unsigned new_block_size = h->size - sizeof(struct heap_block_header) - size;
                            struct heap_block_header* new_block = create_block(heap, new_block_address, new_block_size);
                            new_block->next = h->next;

                            h->size = sizeof(struct heap_block_header) + size;
                            h->next = new_block;
                            set_allocated(heap, h);

                            write_canary(h);

                            assert(new_block->size + h->size == old_size);

                            bool unlocked = heap_unlock(heap);
                            assert(unlocked);

                            return h;
                        } else {
                            set_allocated(heap, h);

                            write_canary(h);

                            bool unlocked = heap_unlock(heap);
                            assert(unlocked);

                            return h;
                        }
                    }
                } else {
                    while(data1 - header0 < sizeof(struct heap_block_header) * 2) {
                        data1 += alignment;
                    }
                    assert(data1 - header0 >= sizeof(struct heap_block_header) * 2);

                    unsigned total_size = h->size;
                    if(total_size > (sizeof(struct heap_block_header) * 2) + size) {
                        unsigned char* header1 = data1 - sizeof(struct heap_block_header);

                        h->size = header1 - header0;

                        set_free(heap, h);
                        assert(is_valid_block(heap, h));

                        struct heap_block_header* h2 = create_block(heap, header1, total_size - h->size);

                        h2->next = h->next;
                        h->next = h2;

                        assert(is_valid_block(heap, h2));

                        assert(h->size + h2->size == total_size);

                        if(h2->size > size + (sizeof(struct heap_block_header) * 2)) {
                            unsigned char* header3 = header1 + size + sizeof(struct heap_block_header);
                            unsigned size3 = h2->size - size - sizeof(struct heap_block_header);
                            struct heap_block_header* h3 = create_block(heap, header3, size3);
                            set_free(heap, h3);

                            h3->next = h2->next;
                            h2->next = h3;
                            h2->size = size + sizeof(struct heap_block_header);

                            assert(is_valid_block(heap, h3));
                            assert(h->size + h2->size + h3->size == total_size);
                        }

                        set_allocated(heap, h2);
                        assert(is_valid_block(heap, h2));

                        write_canary(h2);

                        bool unlocked = heap_unlock(heap);
                        assert(unlocked);

                        return h2;
                    }
                }
            }
        }

        /* No fitting block found, add more memory to heap */
        struct heap_block_header* new_header = heap_grow(heap, size);
        if(!new_header)
            break;
    }

    bool unlocked = heap_unlock(heap);
    assert(unlocked);

    return NULL;
}

struct heap_block_header* heap_alloc_block(struct heap* heap, unsigned size)
{
    struct heap_block_header* result = heap_alloc_block_aligned(heap, size, sizeof(uint64_t));
    check_canary(result);
    return result;
}

void heap_free_block(struct heap* heap, struct heap_block_header* block)
{
    if(!block)
        return;

    if(block->magic == MAGIC_FREE) {
        panic("Double-free detected at block %p", block);
    } else if(block->magic != MAGIC_ALLOCATED) {
        panic("Trying to free invalid block at %p (magic: %p)", block, block->magic);
    }

    check_canary(block);

    bool locked = heap_lock(heap);
    if(!locked) {
        panic("Concurrent heap modification detected");
    }

    assert(is_valid_block(heap, block));
    assert(heap_is_allocated(heap, block));
    set_free(heap, block);

    /* If previous block free, coalesce with previous block */
    struct heap_block_header* prevh = prev_block(heap, block);
    if(prevh && heap_is_free(heap, prevh)) {
        block = merge_blocks(heap, prevh, block);
    }

    if(block->next && heap_is_free(heap, block->next)) {
        block = merge_blocks(heap, block, block->next);
    }

    bool unlocked = heap_unlock(heap);
    assert(unlocked);
}

void heap_dump(struct heap* heap)
{
    trace("Heap dump:");
    for(struct heap_block_header* h = heap->head; h; h = h->next) {
        trace("\t%p: size: %d, allocated: %s, next: %p",
              h,
              h->size,
              heap_is_allocated(heap, h) ? "true" : "false",
              h->next);
    }

}

bool heap_lock(struct heap* heap)
{
    bool result = spinlock_try_lock(&heap->lock);
    return result;
}

bool heap_unlock(struct heap* heap)
{
    bool result = spinlock_try_unlock(&heap->lock);
    return result;
}

void heap_check(struct heap* heap, const char* file, int line)
{
    for(struct heap_block_header* h = heap->head; h; h = h->next) {
        if(!is_valid_block(heap, h)) {
            panic("Invalid block detected at %s:%d",
                  file, line);
        }
    }
}

void* heap_alloc_aligned(struct heap* heap, unsigned size, unsigned alignment)
{
    void* result = NULL;
    struct heap_block_header* hdr = heap_alloc_block_aligned(heap, size, alignment);
    if(hdr) {
        result = (unsigned char*)hdr + sizeof(struct heap_block_header);
    }
    assert(hdr->magic == MAGIC_ALLOCATED);
    return result;
}

void* heap_alloc(struct heap* heap, unsigned size)
{
    void* result = heap_alloc_aligned(heap, size, sizeof(uint64_t));
    return result;
}

void heap_free(struct heap* heap, void* ptr)
{
    struct heap_block_header* hdr = (struct heap_block_header*)
        ((unsigned char*)ptr - sizeof(struct heap_block_header));
    heap_free_block(heap, hdr);
}




