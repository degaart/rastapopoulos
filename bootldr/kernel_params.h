#ifndef _KERNEL_PARAMS_H_
#define _KERNEL_PARAMS_H_

struct kernel_params {
	uint8_t boot_drive;
	uint8_t unused0;
	uint16_t memmap_size;
	uint32_t kernel_entry;
	uint32_t memmap[];
} __attribute__((packed));

#endif //_KERNEL_PARAMS_H_
