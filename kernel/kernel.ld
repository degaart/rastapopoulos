STARTUP(obj/kstub.asm.o)
/*OUTPUT_FORMAT("binary")*/
ENTRY(_kernel_entry);
OUTPUT_FORMAT("elf32-i386");

SECTIONS
{
	/*
		Put a space after the section name and before the colon
	*/
    . = 0xC0100000;

    _KERNEL_START_ = .;
    _TEXT_START_ = .;
	.text : AT (ADDR (.text) - 0xC0000000)
	{
		*(.text)
	}
    _TEXT_END_ = .;

	. = ALIGN(4096);
    _RODATA_START_ = .;
	.rodata : AT (ADDR (.rodata) - 0xC0000000)
	{
		*(.rodata*)
	}
    _RODATA_END_ = .;
	
	. = ALIGN(4096);
    _DATA_START_ = .;	
	.data : AT (ADDR (.data) - 0xC0000000)
	{
		*(.data)
	}
    _DATA_END_ = .;
    
	. = ALIGN(4096);
    _BSS_START_ = .;
	.bss : AT (ADDR (.bss) - 0xC0000000)
	{
		*(.bss)
	}
    _BSS_END_ = .;

	_KERNEL_END_ = .;
}




