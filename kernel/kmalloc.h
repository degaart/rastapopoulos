#ifndef _KMALLOC_H_
#define _KMALLOC_H_

#include <stdint.h>

void* kmalloc_ap(uint32_t size, unsigned alignment, uint32_t* physical);
void* kmalloc_ap(uint32_t size, uint32_t* physical);
void* kmalloc(uint32_t size);
void kfree(void* ptr);
uint32_t kheap_start();

#endif
