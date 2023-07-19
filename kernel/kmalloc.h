#pragma once

#include <stddef.h>
#include <stdbool.h>

extern bool early_kmalloc_enabled;
void* early_kmalloc(size_t size);
void* early_kmalloc_aligned(size_t size, size_t alignment);
void* early_kmalloc_get_heap(void);

extern void* kmalloc_heap;
extern void* kmalloc_heap_end;

void*  kmalloc(size_t);
void   kfree(void*);
void*  kcalloc(size_t, size_t);
void*  krealloc(void*, size_t);
void*  krealloc_in_place(void*, size_t);
void*  kmemalign(size_t, size_t);
int    kposix_memalign(void**, size_t, size_t);
void*  kvalloc(size_t);
int    kmallopt(int, int);
size_t kmalloc_footprint(void);
size_t kmalloc_max_footprint(void);
size_t kmalloc_footprint_limit();
size_t kmalloc_set_footprint_limit(size_t bytes);
void   kmalloc_inspect_all(void(*handler)(void*, void *, size_t, void*),
                           void* arg);
struct mallinfo kmallinfo(void);
void** kindependent_calloc(size_t, size_t, void**);
void** kindependent_comalloc(size_t, size_t*, void**);
size_t kbulk_free(void**, size_t n_elements);
void*  kpvalloc(size_t);
int    kmalloc_trim(size_t);
void   kmalloc_stats(void);
size_t kmalloc_usable_size(void*);

