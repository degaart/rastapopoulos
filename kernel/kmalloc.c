#include "kmalloc.h"
#include "kernel.h"
#include "pmm.h"
#include "util.h"
#include "heap.h"
#include "debug.h"
#include "registers.h"
#include "locks.h"

struct heap* kernel_heap = NULL;

#define RecordTypeKmalloc       1
#define RecordTypeKfree         2

struct record {
    void* buffer;
    uint16_t size;      // Expressed as log2
    uint8_t type;
    uint8_t align;
} __attribute((packed))__;
static struct record records[16384] = {0};
static struct record* record_ptr = records;
static bool record_enabled = false;
#define MAX_RECORDS countof(records)

static void add_record(int type,  size_t size, int align, void* buffer)
{
    if(record_enabled) {
        assert(record_ptr < records + MAX_RECORDS);

        record_ptr->type = type;

        if(type == RecordTypeKmalloc && !is_pow2(align)) {
            panic("Invalid alignment: %d", align);
        }
        record_ptr->align = log2(align);

        assert(size < 65536);
        record_ptr->size = size;
        record_ptr->buffer = buffer;
        record_ptr++;
    }
}

void kmalloc_init()
{
    /* 
     * We place an initial kernel heap at KERNEL_END - (4Mb - 1)
     */
    unsigned char* heap_start = (unsigned char*)ALIGN(KERNEL_END, PAGE_SIZE);
    unsigned heap_limit = (4 * 1024 * 1024) - (unsigned)heap_start - 1;
    kernel_heap = heap_init(heap_start, PAGE_SIZE * 64, heap_limit);
}

void* kmalloc(unsigned size)
{
    assert(size);

    void* result = kmalloc_a(size, sizeof(uint64_t));

    return result;
}

void kfree(void* address)
{
    enter_critical_section();

    if(address) {
        struct heap_block_header* block =
            (struct heap_block_header*)((unsigned char*)address - sizeof(struct heap_block_header));
        heap_free_block(kernel_heap, block);
    }

    add_record(RecordTypeKfree, 0, 0, address); 
    leave_critical_section();
}

void* kmalloc_a(unsigned size, unsigned alignment)
{
    assert(size);

    enter_critical_section();

    unsigned char* result = NULL;
    struct heap_block_header* block = heap_alloc_block_aligned(kernel_heap,
                                                               size,
                                                               alignment);
    if(!block)
        panic("Kernel heap exhausted");

    if(block) {
        result = ((unsigned char*)block) + sizeof(struct heap_block_header);
    }

    add_record(RecordTypeKmalloc, size, alignment, result);
    leave_critical_section();

    return result;
}

void kernel_heap_info(struct kernel_heap_info* buffer)
{
    enter_critical_section();

    struct heap_info hi = heap_info(kernel_heap);
    buffer->heap_start = (uint32_t)hi.address;
    buffer->heap_size = hi.size;

    leave_critical_section();
}

void heap_record_start()
{
    trace("Starting heap recording. MAX_RECORDS: %d", MAX_RECORDS);
    record_enabled = true;
    record_ptr = records;
}

void heap_record_stop()
{
    record_enabled = false;
}



