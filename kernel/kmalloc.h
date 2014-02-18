#ifndef _KMALLOC_H_
#define _KMALLOC_H_

	void* kmalloc_seg_a(unsigned size, unsigned alignment);
	void* kmalloc_seg(unsigned el_count, unsigned el_size);
	void kmalloc_dump_memmap();
	void kmalloc_init();

#endif //_KMALLOC_H_

