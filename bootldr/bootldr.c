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

void cstart() {
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
	
	
	
	_halt();
}

