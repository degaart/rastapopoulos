#ifndef _KERNEL_PARAMS_H_
#define _KERNEL_PARAMS_H_

struct bios_memmap_t {
    uint32_t base_lo;
    uint32_t base_hi;
    uint32_t size_lo;
    uint32_t size_hi;
    uint32_t flags;
    uint32_t ext_flags;
} __attribute__((packed));

struct kernel_params {
	uint8_t boot_drive;
	uint8_t unused0;
	uint16_t memmap_size;
	uint32_t kernel_entry;
	struct bios_memmap_t memmap[16];
} __attribute__((packed));

#endif //_KERNEL_PARAMS_H_
