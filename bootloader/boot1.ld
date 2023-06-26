OUTPUT_FORMAT(binary)
STARTUP(boot1.o)
SECTIONS
{
    . = 0x07E00;
    .text :
    {
        *(.text);
    }
    .data :
    {
        *(.data);
        *(.bss);
        *(.rodata);
    }
    _heap = ALIGN(4);
}

