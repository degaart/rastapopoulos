#ifndef _PMM_H_
#define _PMM_H_

	void pmm_init();
	uint32_t pmm_alloc_page();
	void pmm_free(uint32_t location);
	uint32_t pmm_alloc_range(uint32_t pages);
	void pmm_free_range(uint32_t location, uint32_t range);
	uint32_t pmm_get_free();

#endif //_PMM_H_
