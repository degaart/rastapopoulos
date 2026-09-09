OUTPUT_FORMAT(binary)
ENTRY(_start)

SECTIONS
{
    /* Address where the binary will be loaded. */
    . = 0x0000;

    __image_start = .;

    .text : ALIGN(1)
    {
        *(.text.startup)
        *(.text.startup.*)
        *(.text)
        *(.text.*)
    }

    .rodata : ALIGN(1)
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data : ALIGN(1)
    {
        *(.data)
        *(.data.*)
    }

    __image_end = .;

    /*
     * BSS is not stored in the flat file. Startup code must clear the
     * range [__bss_start, __bss_end).
     */
    .bss (NOLOAD) : ALIGN(2)
    {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
        __bss_size = __bss_end - __bss_start;
    }

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}

