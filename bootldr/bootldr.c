/*
	Rastapopoul OS
	Second-stage bootloader
 	Loads kernel into 0x100000 from boot drive
	Setup protected mode
	Call kernel

 Memory layout:
	 0x0500 - 0x05FF		: kernel params
	 	0x500	uint16_t	: boot device
	 	0x502	uint16_t	: memmap size
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
uint16_t* memmap_size = (uint16_t*)0x502;
uint32_t* memmap_start = (uint32_t*)0x508;

struct GDT_ENTRY gdt[5];

/*
	Interrupt 0 handler
*/
void isr0() {
	write_string("EXCEPTION: Division by zero\r\n");
	_halt();
}

struct GDT_ENTRY encode_gdt(uint32_t base, uint32_t limit, uint32_t type) {
	struct GDT_ENTRY entry;
	
    // Check the limit to make sure that it can be encoded
    if ((limit > 65536) && ((limit & 0xFFF) != 0xFFF)) {
    	write_string("Invalid GDT limit: ");
    	write_uint32(limit);
    	_halt();
    }
    if (limit > 65536) {
        // Adjust granularity if required
        limit = limit >> 12;
        entry.v[6] = 0xC0;
    } else {
        entry.v[6] = 0x40;
    }
 
    // Encode the limit
    entry.v[0] = limit & 0xFF;
    entry.v[1] = (limit >> 8) & 0xFF;
    entry.v[6] |= (limit >> 16) & 0xF;
 
    // Encode the base 
    entry.v[2] = base & 0xFF;
    entry.v[3] = (base >> 8) & 0xFF;
    entry.v[4] = (base >> 16) & 0xFF;
    entry.v[7] = (base >> 24) & 0xFF;
 
    // And... Type
    entry.v[5] = type;
    
    return(entry);
}

static void get_memmap() {
	int cookie;
	int size;
	uint32_t* buffer;
	int count;
	
	cookie = 0;
	buffer = memmap_start;
	count = 0;
	while(1) {
		for(int i=0; i<6; i++)
			buffer[i] = 0;
		size = 20;
		if(!_get_memmap(buffer, &size, &cookie)) {
			write_string("ERROR: Could not get memory map\r\n");
			_halt();
		}
		if(buffer[2]) {
			buffer += 6;
			count++;
		}
		
		if(!cookie)
			break;
		
	}
	*memmap_size = count;
}

static void dump_memmap() {
	uint32_t* buffer = memmap_start;
	for(int i=0; i<*memmap_size; i++) {
		write_string("    BASE: ");
		write_uint32(buffer[0]);
		write_string(" LENGTH: ");
		write_uint32(buffer[2]);
		write_string(" TYPE: ");
		write_uint32(buffer[4]);
		write_string("\r\n");
		buffer += 6;
	}
}

void cstart() {
	/* Store boot device */
	boot_device = *pboot_device;
	
	/* Get memory map */
	write_string("Getting memory map\r\n");
	get_memmap();
	
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
	gdt[0] = encode_gdt(0, 0, 0);
	gdt[1] = encode_gdt(0, 0xFFFFFFFF, 0x9A);
	gdt[2] = encode_gdt(0, 0xFFFFFFFF, 0x92);
	gdt[3] = encode_gdt(0, 0xFFFFFFFF, 0xFA);
	gdt[4] = encode_gdt(0, 0xFFFFFFFF, 0xF2);
	
	_enter_pmode(gdt, sizeof(gdt)/sizeof(*gdt), kernel_load_area);
	_halt();
}


