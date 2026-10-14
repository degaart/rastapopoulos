#include "early_malloc.h"
#include "bitset.h"
#include "kernel.h"
#include "trace.h"

#define ALIGNMENT sizeof(uint64_t)

static void* early_arena = __kernel_end;

void early_malloc_init(const struct Bitset* bootstrap_memmap)
{
    size_t memmap_end = bitset_find(bootstrap_memmap);
    if (memmap_end == SIZE_MAX)
        panic("Out of memory");

    early_arena = (void*)((memmap_end * 4096) + KERNEL_OFFSET);
    trace("Early arena: 0x%p", early_arena);
}

void* early_malloc(size_t size)
{
    void* result = (void*)ALIGN_UP((uintptr_t)early_arena, ALIGNMENT);
    early_arena = result + size;
    return result;
}

void* early_malloc_tail(void)
{
    return early_arena;
}

void early_free(void*)
{
}

