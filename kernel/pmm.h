#ifndef _PMM_H_
#define _PMM_H_

	void pmm_init();
	uint32_t pmm_alloc_page();
	void pmm_free(uint32_t location);
	uint32_t pmm_alloc_range(uint32_t pages);
	void pmm_free_range(uint32_t location, uint32_t range);
	uint32_t pmm_get_free();
	void pmm_dump_bios_memmap();
	void pmm_dump_mem_regions();
	uint32_t pmm_page_usable(uint32_t location);
	void pmm_reserve(uint32_t location);

#endif //_PMM_H_
