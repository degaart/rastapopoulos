ENTRY(_start)

SECTIONS
{
    /* Load address */
    . = 1M;

    __kernel_start = .;
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
    }

    .data BLOCK(4K) : ALIGN(4K)
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

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }
    __kernel_end = .;
}

