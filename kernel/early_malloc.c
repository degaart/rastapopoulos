#include "early_malloc.h"
#include "kernel.h"

#define ALIGNMENT sizeof(uint64_t)

extern char __kernel_end[];
static void* early_arena = __kernel_end;

void early_malloc_init(void)
{
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

