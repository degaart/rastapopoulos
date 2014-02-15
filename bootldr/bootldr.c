/*
	Rastapopoul OS
	Second-stage bootloader
 	Loads kernel into 0x100000 from boot drive
	Setup protected mode
	Call kernel

 Memory layout:
	 0x0500 - 0x05FF		: kernel params
	 0x05FF - 0x7BFF		; bootloader stack
	 0x7C00 - 0x7DFF		: BPB of boot drive
*/
#include "code16gcc.h"
#include <stdint.h>
#include "bootldr_stub.h"
#include "bootldr_str.h"
#include "fat.h"
#include "disk.h"

uint8_t *pboot_device = (uint8_t*)0x500;
uint16_t boot_device;
uint8_t workmem[512];

/*
	Interrupt 0 handler
*/
void isr0() {
	write_string("EXCEPTION: Division by zero\r\n");
	_halt();
}

void cstart() {
	boot_device = *pboot_device;

	/* Check and enable A20 gate */
	write_string("Checking A20 gate\r\n");
	if(!_check_a20()) {
		write_string("Enabling A20 gate\r\n");
		_enable_a20();
		if(!_check_a20()) {
			write_string("ERROR: A20 not enabled\r\n");
			_halt();
		}
	}
	write_string("A20 enabled\r\n");
	
	/* Open fat volume */
	struct FAT fat;
	write_string("Opening boot device\r\n");
	uint16_t ret = fat_open(&fat, boot_device);
	if(ret != FAT_OK) {
		write_string("Error opening boot device: ");
		write_string(fat_error(ret));
		write_string("\r\n");
		_halt();
	}
	
#if 0
	/* DEBUG: CHS->lsect mapping */
	write_string("Calculating lsect\r\n");
	//struct FAT* hfat, uint32_t lsect, uint16_t* cyl, uint16_t* head, uint16_t* sector
	uint32_t lsect = 0x13;
	uint16_t cyl, head, sector;
	
	fat_lsect_to_chs(&fat, lsect, &cyl, &head, &sector);
	DUMP16(lsect);
	DUMP16(cyl);
	DUMP16(head);
	DUMP16(sector);
	_halt();
#endif

#if 0
	/* DEBUG: Read sector 0-2879 */
	for(uint16_t lsect=0; lsect<2880; lsect++) {
		ret = fat_read_lsect(workmem, &fat, lsect);
		if(ret != FAT_OK) {
			write_string("Error reading lset ");
			write_uint16(lsect);
			write_string(": ");
			write_string(fat_error(ret));
			write_string("\r\n");
			
			
			uint16_t cyl, head, sector;
			fat_lsect_to_chs(&fat, lsect, &cyl, &head, &sector);
			DUMP16(lsect);
			DUMP16(cyl);
			DUMP16(head);
			DUMP16(sector);
			
			ret = disk_read_chs(workmem, fat.device, cyl, head, sector);
			if(ret != FAT_OK) {
				write_string("Error reading disk using CHS\r\n");
			}
			_halt();
		}
	}
	_halt();
#endif

#if 0
	/* DEBUG: Read CHS 0, 1, 1 */
	/*
	uint16_t disk_read_chs(
		void* buffer,
		uint16_t device,
		uint16_t c,
		uint16_t h,
		uint16_t s
	);*/
	ret = disk_read_chs(workmem, 0, 0, 1, 1);
	if(ret != FAT_OK) {
		write_string("Error reading CHS 0,1,1\r\n");
	}
	_halt();
#endif

#if 0
	/* DEBUG: Read lsect 0x12 */
	ret = fat_read_lsect(workmem, &fat, 0x12);
	if(ret != FAT_OK) {
		write_string("Error reading lsect 0x12: ");
		write_string(fat_error(ret));
		_halt();
	}
#endif
	
	
	/* Open kernel */
	struct FAT_FILE kernel_file;
	write_string("Opening kernel\r\n");
	ret = fat_fopen(&kernel_file, &fat, "KERNEL     ");
	if(ret != FAT_OK) {
		write_string("Error opening kernel: ");
		write_string(fat_error(ret));
		write_string("\r\n");
		_halt();
	}
	write_string("kernel opened\r\n");
	_halt();
}

