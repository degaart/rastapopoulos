/*
	Rastapopoul OS
	Second-stage bootloader
 	Loads kernel into 0x100000 from boot drive
	Setup protected mode
	Call kernel

 Memory layout:
	 0x0500 - 0x05FF		: kernel params
	 	0x500				: boot device (BYTE)
	 	0x502				: memmap_size (uint16_t)
	 	0x504				: kernel load area (uint32_t)
	 0x05FF - 0x7BFF		; bootloader stack
	 0x7C00 - 0x7DFF		: BPB of boot drive
	 0x100000 - ?			: kernel load area
*/
#include <stdint.h>
#include "bootldr_stub.h"
#include "bootldr_str.h"
#include "fat.h"
#include "disk.h"
#include "debug.h"
#include "kernel_params.h"

struct kernel_params* kernel_params = (struct kernel_params*)0x500; /* boot_drive set by bootsect */
uint8_t workmem[512];
uint8_t* kernel_load_area = (uint8_t*)0x100000;

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

uint32_t _bios_memmap(void*,uint32_t);
static void get_memmap() {
	//bzero(kernel_params->memmap, sizeof(kernel_params->memmap));
	uint32_t ret = _bios_memmap(kernel_params->memmap, sizeof(kernel_params->memmap));
	if(ret == UINT32_MAX) {
		TRACE("ERROR: Failed to get memory map");
		_halt();
	}
	TRACE("_bios_memmap ret: %d", ret);
	kernel_params->memmap_size = ret;
}

static void dump_memmap() {
	struct bios_memmap_t* buffer = kernel_params->memmap;

	TRACE("Memory regions:");
	for(int i=0; i<kernel_params->memmap_size; i++) {
		if(buffer[i].base_hi == 0) {
			TRACE(
				"\tBASE: 0x%X LENGTH: 0x0x%X TYPE: 0x%X", 
				buffer[i].base_lo, 
				buffer[i].size_lo, 
				buffer[i].flags
			);	
		}
	}
}

void cstart() {
	TRACE("*** RastapopoulOS bootloader started ***");
	TRACE("Boot device: 0x%X", kernel_params->boot_drive);
	
	/* Get memory map */
	TRACE("Getting memory map");
	write_string("Getting memory map\r\n");
	get_memmap();
	dump_memmap();
	
	/* Check and enable A20 gate */
	TRACE("Checking A20");
	write_string("Checking A20 gate\r\n");
	if(!_check_a20()) {
		TRACE("Enabling A20");
		write_string("Enabling A20 gate\r\n");
		_enable_a20();
		if(!_check_a20()) {
			TRACE("Error enabling A20");
			write_string("ERROR: A20 not enabled\r\n");
			_halt();
		}
	}
	write_string("A20 enabled\r\n");
	
	/* Open fat volume */
	struct FAT fat;
	TRACE("Opening boot device");
	write_string("Opening boot device\r\n");
	uint16_t ret = fat_open(&fat, kernel_params->boot_drive);
	if(ret != FAT_OK) {
		TRACE("Error opening boot device: %s", fat_error(ret));
		write_string("Error opening boot device: ");
		write_string(fat_error(ret));
		write_string("\r\n");
		_halt();
	}

	/* Open kernel */
	struct FAT_FILE kernel_file;
	TRACE("Opening kernel");
	write_string("Opening kernel\r\n");
	ret = fat_fopen(&kernel_file, &fat, "KERNEL     ");
	//ret = fat_fopen(&kernel_file, &fat, "BOOTLDR    ");
	if(ret != FAT_OK) {
		TRACE("Error opening kernel: %s", fat_error(ret));
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
	TRACE("Loading kernel");
	write_string("Loading kernel\r\n");	
	uint8_t *kernel_pointer = kernel_load_area;
	while(1) {
		/* Read into conventional memory */
		ret = fat_fread(workmem, &fat, &kernel_file);
		if(ret == FAT_EOF)
			break;
		else if(ret != FAT_OK) {
			TRACE("Error loading kernel: %s", fat_error(ret));
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
	TRACE("Entering protected mode");
	write_string("Entering all-glorious 32-bit flat address-space protected mode\r\n");
	gdt[0] = encode_gdt(0, 0, 0);
	gdt[1] = encode_gdt(0, 0xFFFFFFFF, 0x9A);
	gdt[2] = encode_gdt(0, 0xFFFFFFFF, 0x92);
	gdt[3] = encode_gdt(0, 0xFFFFFFFF, 0xFA);
	gdt[4] = encode_gdt(0, 0xFFFFFFFF, 0xF2);
	
	_enter_pmode(gdt, sizeof(gdt)/sizeof(*gdt));

	TRACE("Error entering protected mode, halting");
	_halt();
}


