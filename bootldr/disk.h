#ifndef _DISK_H_
#define _DISK_H_

	uint16_t disk_read_chs(
		void* buffer,
		uint16_t device,
		uint16_t c,
		uint16_t h,
		uint16_t s
	);

#endif
