#include "../debug.h"
#include "../util.h"
#include "../vmm.h"
#include "../pmm.h"
#include "../string.h"
#include "../heap.h"
#include "../kmalloc.h"


bool heap_is_free(struct heap* heap, struct heap_block_header* block);
bool heap_is_allocated(struct heap* heap, struct heap_block_header* block);

static void test_simple_alloc(struct heap* heap)
{
    trace("Testing allocs with default alignment");

    /* Allocate 4 blocks of 4 bytes */
    struct heap_block_header* blocks[4];
    for(int i = 0; i < sizeof(blocks) / sizeof(blocks[4]); i++) {
        trace("Allocating block %d", i);
        blocks[i] = heap_alloc_block(heap, sizeof(uint32_t));
        assert(heap_is_allocated(heap, blocks[i]));
    }

    /* Free those 4 blocks */
    trace("Freeing block 0");
    heap_free_block(heap, blocks[0]);
    assert(heap_is_allocated(heap, blocks[1]));
    assert(heap_is_allocated(heap, blocks[2]));
    assert(heap_is_allocated(heap, blocks[3]));

    trace("Freeing block 1");
    heap_free_block(heap, blocks[1]);
    assert(heap_is_allocated(heap, blocks[2]));
    assert(heap_is_allocated(heap, blocks[3]));

    trace("Freeing block 2");
    heap_free_block(heap, blocks[2]);
    assert(heap_is_allocated(heap, blocks[3]));

    trace("Freeing block 3");
    heap_free_block(heap, blocks[3]);

    /* Normally, if we allocate again, we should have the same addresses as result */
    struct heap_block_header* new_blocks[4];
    for(int i = 0; i < sizeof(new_blocks) / sizeof(new_blocks[4]); i++) {
        trace("Allocating block %d again", i);
        new_blocks[i] = heap_alloc_block(heap, sizeof(uint32_t));
        assert(heap_is_allocated(heap, blocks[i]));
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
    heap_free_block(heap, blocks[1]);
    heap_free_block(heap, blocks[3]);
    heap_free_block(heap, blocks[2]);
    
    new_blocks[1] = heap_alloc_block(heap, sizeof(uint32_t) * 4);
    assert(new_blocks[1] == blocks[1]);

    /* Free all these allocated blocks so we have a clean heap again */
    heap_free_block(heap, blocks[0]);
    heap_free_block(heap, new_blocks[1]);
    assert(heap->size == PAGE_SIZE);

    /* Test heap growing code by allocating 4096 * 2 bytes */
    struct heap_block_header* big_block = heap_alloc_block(heap, PAGE_SIZE * 2);
    assert(big_block != NULL);
    assert(big_block->size == sizeof(struct heap_block_header) + (PAGE_SIZE * 2));
    
    struct heap_info hi = heap_info(heap);
    assert(hi.size == PAGE_SIZE * 3);

    /* Which means we can basically allocate (4096 * 3) - (sizeof(struct heap_block_header) * 2) - (4096 * 2)
     * whithout growing the heap
     */
    unsigned alloc_size = (PAGE_SIZE * 3) - (sizeof(struct heap_block_header) * 2) - (PAGE_SIZE * 2);
    struct heap_block_header* small_block = heap_alloc_block(heap, alloc_size);

    hi = heap_info(heap);
    assert(hi.size == PAGE_SIZE * 3);

    /* Free them to clean up heap */
    heap_free_block(heap, small_block);
    heap_free_block(heap, big_block);

    /* 
     * Aaaaand we can then alloc (4096 * 3) - sizeof(struct heap_block_header)
     * without growing the heap
     */
    alloc_size = (PAGE_SIZE * 3) - sizeof(struct heap_block_header);
    big_block = heap_alloc_block(heap, alloc_size);

    hi = heap_info(heap);
    assert(hi.size == PAGE_SIZE * 3);

    heap_free_block(heap, big_block);
}

static void test_aligned_alloc(struct heap* heap)
{
    trace("Testing aligned allocations");

    /*
     * Allocating a block with a 64-byte alignment
     * should suffice to trigger our alignment code path
     */
    struct heap_block_header* b0 = heap_alloc_block_aligned(heap, 64, 64);
    assert(IS_ALIGNED((uint32_t)b0 + sizeof(struct heap_block_header), 64));
    assert(b0->size == sizeof(struct heap_block_header) + 64);
    heap_free_block(heap, b0);

    struct heap_block_header* b1 = heap_alloc_block_aligned(heap, 3, 4);
    assert(IS_ALIGNED((uint32_t)b1 + sizeof(struct heap_block_header), 4));

    struct heap_block_header* b2 = heap_alloc_block_aligned(heap, 3, 4);
    assert(IS_ALIGNED((uint32_t)b2 + sizeof(struct heap_block_header), 4));

    struct heap_block_header* b3 = heap_alloc_block_aligned(heap, 12233 - sizeof(struct heap_block_header) - 32, 4);
    assert(IS_ALIGNED((uint32_t)b3 + sizeof(struct heap_block_header), 4));
}

static void test_heap_limits(struct heap* heap)
{
    trace("Testing heap limits");

    struct heap_block_header* b0 = heap_alloc_block(heap, PAGE_SIZE);
    assert(b0);

    struct heap_block_header* b1 = heap_alloc_block(heap, PAGE_SIZE);
    assert(b1);

    struct heap_block_header* b2 = heap_alloc_block(heap, PAGE_SIZE);
    assert(!b2);
}

void test_kmalloc()
{
    trace("Testing kmalloc()");
    /* Create heap at 48Mb */
    unsigned char* heap_start = (unsigned char*)0x3000000;
    vmm_map(heap_start, pmm_alloc(), VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE);

    struct heap* heap = heap_init(heap_start, PAGE_SIZE, PAGE_SIZE * 3);
    assert(heap_is_free(heap, heap->head));

    assert(heap->head->next == NULL);

    /* Test allocs with default alignment */
    test_simple_alloc(heap);

    /* Reinitialize heap */
    struct heap_info hi = heap_info(heap);
    assert(hi.size == PAGE_SIZE * 3);
    heap = heap_init(heap_start, PAGE_SIZE * 3, PAGE_SIZE * 3);

    /* Test aligned allocs */
    hi = heap_info(heap);
    assert(hi.size == PAGE_SIZE * 3);
    heap = heap_init(heap_start, PAGE_SIZE * 3, PAGE_SIZE * 3);
    test_aligned_alloc(heap);

    /* Test heap limits */
    hi = heap_info(heap);
    assert(hi.size == PAGE_SIZE * 3);
    heap = heap_init(heap_start, PAGE_SIZE * 3, PAGE_SIZE * 3);
    test_heap_limits(heap);

#ifdef ENABLE_COVERAGE
    /* Dump coverage points */
    trace("Test coverage points:");
    for(int i = 0; i < countof(coverage_points) && coverage_points[i].line; i++)
        trace("\t%d %s", coverage_points[i].line, coverage_points[i].desc);
#endif

    /* Clean up paged memory */
    hi = heap_info(heap);
    for(unsigned char* page = heap_start; page < heap_start + hi.size; page += PAGE_SIZE) {
        vmm_unmap(page);
    }
}



