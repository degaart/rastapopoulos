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
	 0x100000 - ?			: kernel load area
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
uint8_t* kernel_load_area = (uint8_t*)0x100000;

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

	/* Open kernel */
	struct FAT_FILE kernel_file;
	write_string("Opening kernel\r\n");
	ret = fat_fopen(&kernel_file, &fat, "KERNEL     ");
	//ret = fat_fopen(&kernel_file, &fat, "BOOTLDR    ");
	if(ret != FAT_OK) {
		write_string("Error opening kernel: ");
		write_string(fat_error(ret));
		write_string("\r\n");
		_halt();
	}
	
	/*
		Read kernel to kernel_load_area
		We can use 32-bit offsets since we're in unreal mode
		But the bios still cannot access this memory
	*/
	write_string("Loading kernel\r\n");	
	uint8_t *kernel_pointer = kernel_load_area;
	while(1) {
		/* Read into conventional memory */
		ret = fat_fread(workmem, &fat, &kernel_file);
		if(ret == FAT_EOF)
			break;
		else if(ret != FAT_OK) {
			write_string("Error reading kernel: ");
			write_string(fat_error(ret));
			_halt();
		}
		
		/* Write into extended memory */
		memcpy(kernel_pointer, workmem, 512);
		
		kernel_pointer += 512;
	}

	/*
		Setup protected mode and call kernel
	*/
	write_string("Entering all-glorious 32-bit flat address-space protected mode\r\n");
	
	
	
	_halt();
}




