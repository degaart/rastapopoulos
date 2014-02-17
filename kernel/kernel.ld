OUTPUT_FORMAT("binary")
/*OUTPUT_FORMAT("elf32-i386")*/
ENTRY(_kstart)
/*STARTUP(_kstart)*/
phys = 0x00100000;
SECTIONS
{
  .text phys : AT(phys) {
    code = .;
    *(.text)
    *(.rodata)
    . = ALIGN(4);
  }
  .data : AT(phys + (data - code))
  {
    data = .;
    *(.data)
    . = ALIGN(4);
  }
  _bss_start = .;
  .bss : AT(phys + (bss - code))
  {
    bss = .;
    *(.bss)
    . = ALIGN(4);
  }
  _kernel_end = .;
  
  end = .;
}
