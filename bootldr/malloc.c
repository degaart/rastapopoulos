#include "malloc.h"
#include "debug.h"

extern unsigned bootldr_end;    /* put here by linked voodoo */
static unsigned char* _heap_start = &bootldr_end;

void dump_heap_start() {
    TRACE("heap start: %p", _heap_start);
}

void* malloca(unsigned size, unsigned align) {
        

}

void* malloc(unsigned size) {
    return malloca(size, 2);
}


