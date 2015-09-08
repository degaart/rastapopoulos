#include <stdint.h>
#include "kterm.h"
#include "kutil.h"
#include "kstub.h"
#include "pmm.h"
#include "pagedir.h"

#if 0
void pagedir_set_current(struct PAGEDIR* pagedir) {
	_write_cr3(pagedir->pagedir);
}

static uint32_t pagedir_get_current() {
	return(_read_cr3());
}

static void paging_enable() {
	_write_cr0(_read_cr0() | CR0_PAGING);
}

static void paging_disable() {
	_write_cr0(_read_cr0() & (~CR0_PAGING));
}

static int paging_enabled() {
	return(_read_cr0() & CR0_PAGING);
}


void pagedir_init(struct PAGEDIR* pagedir) {
	pagedir->pagedir = pmm_alloc_range((sizeof(uint32_t)*1024)/4096);
}

void pagedir_map(
	struct PAGEDIR* pagedir, 
	const void* linear_address, 
	uint32_t physical_address, 
	uint32_t size_bytes,
	uint32_t flags) {

	ASSERT( ((uint32_t)linear_address % 4096) == 0 );
	ASSERT( ((uint32_t)physical_address % 4096) == 0 );
	ASSERT( ((uint32_t)size_bytes % 4096) == 0 );

	/*
		It's so dirty with paging enabled, forgive me!
	*/
	pushf();
	cli();
	
	int paging_was_enabled = paging_enabled();
	if(paging_was_enabled)
		paging_disable();

	uint32_t pagedir_entry = ((uint32_t)linear_address / 4096) / 1024;
	if(!(pagedir->pagedir[pagedir_entry] & PAGE_DIR_PRESENT)) {
		/* Need to allocate new pagetable here */
		uint32_t pagetable = pmm_alloc_page();
		bzero((void*)pagetable, 1024*sizeof(uint32_t));
		pagedir->pagedir[pagedir_entry] = 
			PAGE_DIR_PRESENT|
			PAGE_DIR_PAGETABLE(pmm_alloc_page())|
			PAGE_DIR_USER|
			flags;
	}
	
	uint32_t* pagetable = (uint32_t*)(pagedir->pagedir[pagedir_entry] & PAGE_DIR_PRESENT);
	uint32_t pagetable_entry = (linear_address / 4096) % 1024;
	if(pagetable[pagetable_entry] & PAGE_ENTRY_PRESENT)
		PANIC("Linear address %X is already mapped", linear_address);
	pagetable[pagetable_entry] = 
		PAGE_ENTRY_PRESENT|
		PAGE_ENTRY_BASE(physical_address)|
		flags;

	if(paging_was_enabled)
		paging_enable();
	popf();
}

void pagedir_unmap(struct PAGEDIR* pagedir, const void* linear_address) {
	if(linear_address % 4096)
		PANIC("Invalid linear address to unmap: %X", linear_address);
}

uint32_t pagedir_get_physical(struct PAGEDIR* pagedir, const void* linear_address) {

}


#endif

