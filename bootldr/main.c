/*
    Rastapopoul OS
    Second-stage bootloader
    Loads kernel into 0x100000 from boot drive
    Setup protected mode
    Call kernel

 Memory layout:
     0x0500 - 0x05FF        : kernel params (struct KParams)
     0x05FF - 0x7BFF        ; bootloader stack
     0x7C00 - 0x7DFF        : BPB of boot drive
     0x100000 - ?           : kernel load area
*/
#include "term.h"
#include "util.h"
#include "io.h"
#include "debug.h"
#include "malloc.h"

struct KParams {
    unsigned char boot_drive;   /* Boot drive number, boot here by bootsector */
} *kernel_params = (struct KParams*)0x0500;

/*extern unsigned bootldr_end;*/             /* Put here by linker */

void main() {
    TRACE("*** Rastapopoulos bootloader ***");

    /* Enable A20 Gate */
	TRACE("Enabling A20 gate");
	enable_a20();

    /* Get memory map */
    dump_heap_start();   


    TRACE("Halting");
    halt();
}
