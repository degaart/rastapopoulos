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

    .user_text BLOCK(4K) : ALIGN(4K)
    {
        _user_text_start = .;
        *(.user_text)
        _user_text_end = .;
    }

    .user_data BLOCK(4K) : ALIGN(4K)
    {
        _user_data_start = .;
        *(.user_data)
        _user_data_end = .;
    }

    .user_rodata BLOCK(4K) : ALIGN(4K)
    {
        _user_rodata_start = .;
        *(.user_rodata)
        _user_rodata_end = .;
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        _bss_start = .;
        *(.bss)
        _bss_end = .;
    }
    _heap_start = ALIGN(4096);
}


