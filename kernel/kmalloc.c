#include <stdint.h>
#include "kmalloc.h"
#include "kutil.h"
#include "kterm.h"
#include "ll.h"
#include "kstring.h"

static void* kmalloc_seg_mem_start = (void*)0xFFFFFFFF;

/*
	Allocate, using specified alignment
*/
void* kmalloc_seg_a(unsigned size, unsigned alignment) {
	if(kmalloc_seg_mem_start == (void*)0xFFFFFFFF)
		kmalloc_seg_mem_start = kernel_end;

	/* Begin to allocate memory at mem_start, and align to element size */
	void* allocated_mem = ALIGN(kmalloc_seg_mem_start, alignment);
	if(size >= 0x100000) {
		write_string("WARNING: Trying to allocate ");
		write_uint32(size);
		write_string(" of memory in initial memory manager\n");
	}
	ASSERT((uint32_t)allocated_mem < 0x400000);
	
	/* Now, update mem_start to end of allocated memory */
	kmalloc_seg_mem_start = allocated_mem + size + 1;
	return(allocated_mem);
}

/*
	Very simple kernel allocator, does not permit
	freeing of the allocated memory
	Should only be used when paging disabled
*/
void* kmalloc_seg(unsigned el_count, unsigned el_size) {
	return(kmalloc_seg_a(el_count*el_size, el_size));
}

/*
	Get start of the current zone
*/
void* kmalloc_seg_get_start() {
	return(kmalloc_seg_mem_start);
}


