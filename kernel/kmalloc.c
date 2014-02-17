#include <stdint.h>
#include "kmalloc.h"
#include "kutil.h"

void* kmalloc_a(unsigned size, unsigned alignment) {
	static void* mem_start = (void*)0xFFFFFFFF;
	if(mem_start == (void*)0xFFFFFFFF)
		mem_start = kernel_end;

	/* Begin to allocate memory at mem_start, and align to element size */
	void* allocated_mem = ALIGN(mem_start, alignment);
	
	/* Now, update mem_start to end of allocated memory */
	mem_start = allocated_mem + size + 1;
	return(allocated_mem);
}

/*
	Very simple kernel allocator, does not permit
	freeing of the allocated memory
*/
void* kmalloc(unsigned el_count, unsigned el_size) {
	return(kmalloc_a(el_count*el_size, el_size));
}

