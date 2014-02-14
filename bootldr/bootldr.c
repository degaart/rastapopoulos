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


uint8_t *pboot_device = (uint8_t*)0x500;
uint16_t boot_device;
uint8_t workmem[512];

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
	
	/* DEBUG: CHS->lsect mapping */
	//struct FAT* hfat, uint32_t lsect, uint16_t* cyl, uint16_t* head, uint16_t* sector
	uint32_t lsect = 1;
	uint16_t cyl, head, sector;
	
	breakpoint();
	fat_lsect_to_chs(&fat, lsect, &cyl, &head, &sector);
	write_string("LSECT: ");
	write_uint16(lsect & 0xFFFF);
	write_string(" CHS: ");
	write_uint16(cyl);
	write_string(" ");
	write_uint16(head);
	write_string(" ");
	write_uint16(sector);
	_halt();
	
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
	_halt();
}

