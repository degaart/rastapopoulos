ENTRY(_start)
SECTIONS
{
    . = 1M;

    .text BLOCK(4K) : ALIGN(4K)
    {
        _text_start = .;
        *(.multiboot)
        *(.text)
        _text_end = .;
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        _rodata_start = .;
        *(.rodata)
        *(.comment)
        _rodata_end = .;
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        _data_start = .;
        *(.data)
        _data_end = .;
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        _bss_start = .;
        *(.bss)
        _bss_end = .;
    }
    _heap_start = ALIGN(4096);
}


