#ifndef _KMALLOC_H_
#define _KMALLOC_H_

#include <stdint.h>
#include "util.h"

EXPORT void* kmalloc_ap(uint32_t size, unsigned alignment, uint32_t* physical);
EXPORT void* kmalloc(uint32_t size);
EXPORT void kfree(void* ptr);

#endif
