ENTRY(_start)

SECTIONS {
    . = 1M;
    __KERNEL_START__ = .;

    .text BLOCK(4K) : ALIGN(4K)
    {
        __TEXT_START__ = .;
        *(.multiboot)
        *(.text)
        __TEXT_END__ = .;
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        __RODATA_START__ = .;
        *(.rodata)
        __RODATA_END__ = .;
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        __DATA_START__ = .;
        *(.data)
        __DATA_END__ = .;
    }

    .bss BLOCK(4K): ALIGN(4K)
    {
        __BSS_START__ = .;
        *(COMMON)
        *(.bss)
        __BSS_END__ = .;
    }

    __KERNEL_END__ = .;
}

