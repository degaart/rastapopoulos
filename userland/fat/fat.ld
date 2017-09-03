ENTRY(_entry);
OUTPUT_FORMAT("elf32-i386");

SECTIONS
{
	/*
		Put a space after the section name and before the colon
	*/
    __START__ = .;
    
	.text 0x00C00000 :
	{
		__TEXT_START__ = .;
		*(.text)
		__TEXT_END__ = .;
	}

	. = ALIGN(4096);
	.rodata : 
	{
		__RODATA_START__ = .;
		*(.rodata*)
		__RODATA_END__ = .;
	}
	
	. = ALIGN(4096);
	.data :
	{
		__DATA_START__ = .;	
		*(.data)
		__DATA_END__ = .;
	}

	. = ALIGN(4096);
	.bss :
	{
		__BSS_START__ = .;
		*(.bss)
		__BSS_END__ = .;
	}

    __END__ = .;
}


