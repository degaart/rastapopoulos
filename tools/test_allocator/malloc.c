#include <stddef.h>
#include <string.h>
#include <unistd.h>

struct heap;

struct heap* heap_init(void* start, unsigned size);
void heap_grow(struct heap* heap, void* start, unsigned size);
void* heap_alloc(struct heap* heap, unsigned size);
void heap_free(struct heap* heap, void* address);
unsigned heap_block_size(struct heap* heap, void* block);

unsigned char storage[16777216];
volatile int initialized = 0;
struct heap* heap;

void* malloc( size_t size )
{
    write(STDERR_FILENO, "malloc\n", 7);
    if(!initialized) {
        heap = heap_init(storage, sizeof(storage));
        initialized = 1;
    }

    return heap_alloc(heap, size);
}

void free( void* ptr )
{
    heap_free(heap, ptr);
}

void* calloc( size_t num, size_t size )
{
    void* buffer = malloc(size * num);
    memset(buffer, 0, size * num);
    return buffer;
}

void *realloc( void *ptr, size_t new_size )
{
    void* buffer = malloc(new_size);
    memcpy(buffer, ptr, heap_block_size(heap, ptr));
    return buffer;
}

