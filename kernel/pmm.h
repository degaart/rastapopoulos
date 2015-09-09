#ifndef _PMM_H_
#define _PMM_H_

	void pmm_init();
	uint32_t pmm_alloc_page();
	void pmm_free_page(uint32_t location);
	uint32_t pmm_alloc(uint32_t pages);
	void pmm_free(uint32_t location, uint32_t range);
	uint32_t pmm_get_free();
	void pmm_dump_bios_memmap();
	void pmm_dump_mem_regions();
	uint32_t pmm_page_usable(uint32_t location);
	void pmm_reserve(uint32_t location);
	void pmm_add_region(uint32_t base, uint32_t size, uint32_t type);
	int pmm_page_status(uint32_t location);
	
	#define REGION_FREE						1
	#define REGION_RESERVED					2
	
	#define PMM_STATUS_FREE			0
	#define PMM_STATUS_ALLOCATED	1
	#define PMM_STATUS_RESERVED		2
	#define PMM_STATUS_ABSENT		4

	#define PAGE_SIZE 4096

#endif //_PMM_H_
