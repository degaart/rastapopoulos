#include "cxxrt.h"
#include "debug.h"
#include "kmalloc.h"
#include "string.h"
#include <stddef.h>

void *__dso_handle;
extern "C" int __cxa_atexit(void (*destructor) (void *), void *arg, void *dso) {
	return 0;
}

extern "C" void __cxa_finalize(void *f) {
	TRACE("__cxa_finalize called");
}

extern "C" void __cxa_pure_virtual() {
    TRACE("Pure virtual function call");
    halt();
}

void *operator new(size_t size) {
    void* buffer = kmalloc(size);
    // TRACE("operator new(%u) => %p", size, buffer);

    //bzero(buffer, size);
    return buffer;
}
 
void *operator new[](size_t size) {
    void* buffer = kmalloc(size);
    // TRACE("operator new[](%u) => %p", size, buffer);
    return buffer;
}
 
void operator delete(void *p) throw() {
    kfree(p);
}
 
void operator delete[](void *p) throw() {
    kfree(p);
}


