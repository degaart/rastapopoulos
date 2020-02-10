ENTRY(_start)

SECTIONS {
    . = 0xC0100000;
    //. = 1M;

    __KERNEL_START__ = .;

    .text BLOCK(4K) : AT (ADDR (.text) - 0xC0000000)
    {
        __TEXT_START__ = .;
        *(.multiboot)
        *(.text)
        __TEXT_END__ = .;
    }

    .rodata BLOCK(4K) : AT (ADDR (.rodata) - 0xC0000000)
    {
        __RODATA_START__ = .;
        *(.rodata)
        __RODATA_END__ = .;
    }

    .data BLOCK(4K) : AT (ADDR (.data) - 0xC0000000)
    {
        __DATA_START__ = .;
        *(.data)
        __DATA_END__ = .;
    }

    .bss BLOCK(4K): AT (ADDR (.bss) - 0xC0000000)
    {
        __BSS_START__ = .;
        *(COMMON)
        *(.bss)
        __BSS_END__ = .;
    }

    __KERNEL_END__ = .;
}

