OUTPUT_FORMAT("binary")
ENTRY(_start)

SECTIONS
{
  /*
    Put a space after the section name and before the colon
  */
  .text 0x7E00 :
  {
    _TEXT_START_ = .;
    *(.text)
    *(.rodata)
    _TEXT_END_ = .;
  }
  
  .data :
  {
    _DATA_START_ = .; 
    *(.data)
    _DATA_START_ = .;
  }

  .bss :
  {
    _BSS_START_ = .;
    *(.bss)
    _BSS_END_ = .;
  }
  _bootldr_end = .;
}
