#include "../heap.h"
#include "test.h"

struct HeapBlock;
extern struct HeapBlock* heap_first;
extern struct HeapBlock* heap_last;
extern uintptr_t heap_begin;
extern uintptr_t heap_end;

unsigned char heap_test_memory[32768] __attribute__((aligned(4096)));
size_t heap_test_used;
unsigned heap_testing;

void test_heap(void)
{
    enter_critical_section();
    struct HeapBlock* saved_first = heap_first;
    struct HeapBlock* saved_last = heap_last;
#ifdef HEAP_DEBUG
    uintptr_t saved_begin = heap_begin;
    uintptr_t saved_end = heap_end;
    heap_begin = heap_end = 0;
#endif
    heap_first = heap_last = NULL;
    heap_testing = 1;
    heap_test_used = 0;
    trace("*** Heap tests started");
    heap_free(NULL);
    CHECK(!heap_alloc(0));
    CHECK(!heap_alloc_aligned(1, 3));
    CHECK(!heap_alloc(SIZE_MAX));
    CHECK(!heap_alloc_aligned(SIZE_MAX - 16, 4096));
    CHECK(!heap_alloc_aligned(1, (size_t)1 << 31));
    CHECK(!heap_alloc(sizeof(heap_test_memory)));
    for (size_t alignment = 1; alignment <= 4096; alignment *= 2) {
        for (size_t size = 16; size <= 17; size++) {
            unsigned char* ptr = heap_alloc_aligned(size, alignment);
            CHECK(ptr && !((uintptr_t)ptr % alignment));
            for (size_t i = 0; i < size; i++)
                ptr[i] = (unsigned char)i;
            struct HeapInfo live = heap_info();
            CHECK(live.total_size ==
                  live.free_size + live.allocated_size + live.overhead);
#ifdef HEAP_DEBUG
            CHECK(!heap_test_validate(ptr));
            CHECK(heap_info().allocated_size == size);
            CHECK(heap_test_validate(ptr + 1));
            CHECK(heap_test_validate((void*)1));
            struct HeapBlock* block = ((struct HeapBlock**)ptr)[-1];
            ((struct HeapBlock**)ptr)[-1] = NULL;
            CHECK(heap_test_validate(ptr));
            ((struct HeapBlock**)ptr)[-1] = block;
            size_t saved_size = block->size;
            block->size = SIZE_MAX;
            CHECK(heap_test_validate(ptr));
            block->size = saved_size;
            struct HeapBlock* next = block->next;
            block->next = (void*)1;
            CHECK(heap_test_validate(ptr));
            block->next = next;
            block->prev = block;
            CHECK(heap_test_validate(ptr));
            block->prev = NULL;
            block->requested = SIZE_MAX;
            CHECK(heap_test_validate(ptr));
            block->requested = size;
            block->returned = ptr + 1;
            CHECK(heap_test_validate(ptr));
            block->returned = ptr;
            ptr[size] ^= 1;
            CHECK(heap_test_validate(ptr));
            ptr[size] ^= 1;
            CHECK(!heap_test_validate(ptr));
#endif
            heap_free(ptr);
            CHECK(!heap_test_used);
#ifdef HEAP_DEBUG
            CHECK(heap_test_validate(ptr));
#endif
        }
    }
    void* a = heap_alloc(100);
    void* b = heap_alloc(100);
    void* c = heap_alloc(100);
    CHECK(a && b && c);
    CHECK(heap_first->next && heap_first->next->next);
    struct HeapInfo before_failure = heap_info();
    CHECK(!heap_alloc(sizeof(heap_test_memory)));
    CHECK(heap_info().total_size == before_failure.total_size);
    CHECK(heap_info().allocated_size == before_failure.allocated_size);
    heap_free(b);
#ifdef HEAP_DEBUG
    CHECK(heap_test_validate(b));
#endif
    CHECK(heap_alloc(100) == b);
    heap_free(b);
    heap_free(a);
#ifdef HEAP_DEBUG
    CHECK(heap_test_validate(b));
#endif
    CHECK(heap_alloc(200) == a);
    heap_free(a);
    void* large = heap_alloc(12000);
    CHECK(large);
    size_t before = heap_test_used;
    heap_free(large);
    CHECK(heap_test_used < before);
    heap_free(c);
    CHECK(!heap_test_used);
    struct HeapInfo info = heap_info();
    CHECK(!info.total_size && !info.allocated_size && !info.overhead);
    heap_testing = 0;
    heap_first = saved_first;
    heap_last = saved_last;
#ifdef HEAP_DEBUG
    heap_begin = saved_begin;
    heap_end = saved_end;
#endif
    trace("*** Heap tests done");
    leave_critical_section();
}
