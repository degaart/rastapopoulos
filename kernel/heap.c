#include "heap.h"
#include "util.h"
#include "debug.h"
#include "string.h"
#include "pmm.h"
#include "vmm.h"

#ifdef ENABLE_COVERAGE
#define add_coverage(desc) add_coverage_point(__LINE__, desc)
#else
#define add_coverage(desc)
#endif

#define BLOCK_ALLOCATED             0x1

/*
 * Test coverage points
 */
struct coverage_point {
    int line;
    const char* desc;
};
static struct coverage_point coverage_points[32] = {0};

static void dump_heap(struct heap* heap);


static void add_coverage_point(int line, const char* desc)
{
    int i;
    for(i = 0; 
        i < countof(coverage_points) && coverage_points[i].line && coverage_points[i].line != line; 
        i++) {
    }

    if(i < countof(coverage_points) && !coverage_points[i].line) {
        coverage_points[i].line = line;
        coverage_points[i].desc = desc;
    }
}

static unsigned block_checksum(const struct heap_block_header* block)
{
    struct heap_block_header hdr = *block;
    hdr.checksum = 0;

    unsigned checksum = hash(&hdr, sizeof(hdr));
    return checksum;
}

static void update_checksum(struct heap_block_header* block)
{
    block->checksum = block_checksum(block);
}

static bool is_valid_block(struct heap* heap, struct heap_block_header* header)
{
    unsigned checksum = block_checksum(header);
    bool result = (checksum == header->checksum);
    return result;
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

bool heap_is_free(struct heap* heap, struct heap_block_header* block)
{
    assert(is_valid_block(heap, block));

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
    update_checksum(block);
}

static void set_free(struct heap* heap, struct heap_block_header* block)
{
    assert(is_valid_block(heap, block));

    block->flags &= ~BLOCK_ALLOCATED;
    update_checksum(block);
}

static struct heap_block_header* create_block(struct heap* heap, void* addr, unsigned size)
{
    struct heap_block_header* result = addr;
    memset(result, 0, sizeof(struct heap_block_header));
    result->size = size;
    update_checksum(result);
    return result;
}

static struct heap_block_header* merge_blocks(struct heap* heap, struct heap_block_header* dest, struct heap_block_header* source)
{
    assert(dest < source);

    dest->size += source->size;
    dest->next = source->next;

    memset(source, 0, sizeof(struct heap_block_header));
    update_checksum(dest);

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

struct heap_block_header* heap_grow(struct heap* heap, unsigned size)
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
        update_checksum(last);

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
                    add_coverage("aligned size == data start");

                    if(h->size >= sizeof(struct heap_block_header) + size) {                           /* Size of block is enough for header and data */
                        add_coverage("block size > header + size");

                        unsigned remaining = h->size - sizeof(struct heap_block_header) - size;
                        if(remaining > sizeof(struct heap_block_header)) {                             /* The remaining bytes can be used as a new block */
                            add_coverage("new block can be split");

                            unsigned old_size = h->size;

                            unsigned char* new_block_address = header0 + size + sizeof(struct heap_block_header);
                            unsigned new_block_size = h->size - sizeof(struct heap_block_header) - size;
                            struct heap_block_header* new_block = create_block(heap, new_block_address, new_block_size);
                            new_block->next = h->next;
                            update_checksum(new_block);

                            h->size = sizeof(struct heap_block_header) + size;
                            h->next = new_block;
                            update_checksum(h);
                            set_allocated(heap, h);

                            assert(new_block->size + h->size == old_size);
                            return h;
                        } else {
                            add_coverage("new block cannot be split");

                            set_allocated(heap, h);
                            return h;
                        }
                    }
                } else {
                    add_coverage("3-way split");

                    while(data1 - header0 < sizeof(struct heap_block_header) * 2) {
                        add_coverage("realign");
                        data1 += alignment;
                    }
                    assert(data1 - header0 >= sizeof(struct heap_block_header) * 2);

                    unsigned total_size = h->size;
                    if(total_size > (sizeof(struct heap_block_header) * 2) + size) {
                        add_coverage("fit aligned block");

                        unsigned char* header1 = data1 - sizeof(struct heap_block_header);

                        h->size = header1 - header0;
                        update_checksum(h);

                        set_free(heap, h);
                        assert(is_valid_block(heap, h));

                        struct heap_block_header* h2 = create_block(heap, header1, total_size - h->size);
                        set_allocated(heap, h2);

                        h2->next = h->next;
                        h->next = h2;

                        update_checksum(h);
                        update_checksum(h2);

                        assert(is_valid_block(heap, h2));

                        assert(h->size + h2->size == total_size);

                        if(h2->size > size + (sizeof(struct heap_block_header) * 2)) {
                            add_coverage("aligned block can be split");

                            unsigned char* header3 = header1 + size + sizeof(struct heap_block_header);
                            unsigned size3 = h2->size - size - sizeof(struct heap_block_header);
                            struct heap_block_header* h3 = create_block(heap, header3, size3);
                            set_free(heap, h3);

                            h3->next = h2->next;
                            h2->next = h3;
                            h2->size = size + sizeof(struct heap_block_header);

                            update_checksum(h2);
                            update_checksum(h3);

                            assert(is_valid_block(heap, h3));

                            assert(h->size + h2->size + h3->size == total_size);
                        } else {
                            add_coverage("aligned block cannot be split");
                        }

                        return h2;
                    }
                }
            }
        }

        /* No fitting block found, add more memory to heap */
        add_coverage("grow heap");
        struct heap_block_header* new_header = heap_grow(heap, size);
        if(!new_header)
            break;
    }
    return NULL;
}

struct heap_block_header* heap_alloc_block(struct heap* heap, unsigned size)
{
    struct heap_block_header* result = heap_alloc_block_aligned(heap, size, 4);
    return result;
}

void heap_free_block(struct heap* heap, struct heap_block_header* block)
{
    if(!block)
        return;

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

