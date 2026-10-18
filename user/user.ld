ENTRY(_start)

SECTIONS
{
    /* Load address */
    . = 0x400000;

    .text BLOCK(4) : ALIGN(4)
    {
        *(.start.text .start.text.*)
        *(.text)
    }

    .data BLOCK(4) : ALIGN(4)
    {
        *(.rodata)
        *(.data)
    }

    .init_array : ALIGN(4)
    {
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    }

    __bss_start = .;
    .bss BLOCK(4) : ALIGN(4)
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }
    __bss_end = .;
}

