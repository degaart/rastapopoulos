/*STARTUP(obj/kernel_stub.ao)*/
OUTPUT_FORMAT("binary")

SECTIONS
{
	/*
		Put a space after the section name and before the colon
	*/

	.text 0x00100000 :
	{
		_TEXT_START_ = .;
		*(.text)
		_TEXT_END_ = .;
	}

	.rodata : 
	{
		_RODATA_START_ = .;
		*(.rodata)
		*(.eh_frame)
		_RODATA_END_ = .;
	}
	
	.data :
	{
		_DATA_START_ = .;	
		*(.data)
		_DATA_START_ = .;
	}

	.ctors : 
	{
		_CTORS_START_ = .;
		*(.ctors)
		_CTORS_END_ = .;
	}

	.bss :
	{
		_BSS_START_ = .;
		*(.bss)
		_BSS_END_ = .;
	}
	_kernel_end = .;
}
