ENTRY(_start)

SECTIONS
{
    /* Load address */
    . = 1M;

    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.rodata)
        *(.text)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    .init_array : ALIGN(4)
    {
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
}

