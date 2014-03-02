#ifndef _PROCESS_H_
#define _PROCESS_H_
	
	struct PROCESS {
		uint32_t pagedir;			/* Process pagedir */
		uint32_t eip;
		uint32_t ss;
		uint32_t esp;
	};
	
	void process_create(struct PROCESS* proc, void* address);

#endif //_PROCESS_H_
