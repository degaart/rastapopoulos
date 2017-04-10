ENTRY(_entry);
OUTPUT_FORMAT("elf32-i386");

SECTIONS
{
	/*
		Put a space after the section name and before the colon
	*/
	.text 0x00400000 :
	{
		_TEXT_START_ = .;
		*(.text)
		_TEXT_END_ = .;
	}

	. = ALIGN(4096);
	.rodata : 
	{
		_RODATA_START_ = .;
		*(.rodata*)
		_RODATA_END_ = .;
	}
	
	. = ALIGN(4096);
	.data :
	{
		_DATA_START_ = .;	
		*(.data)
		_DATA_END_ = .;
	}

	. = ALIGN(4096);
	.bss :
	{
		_BSS_START_ = .;
		*(.bss)
		_BSS_END_ = .;
	}
}


