ENTRY(_start)
OUTPUT_FORMAT(binary)

SECTIONS
{
    . = 0x8000;

    __image_start = .;

    .text : ALIGN(2) {
        *(.text.startup)
        *(.text.startup.*)
        *(.text)
        *(.text.*)
    }

    .rodata : ALIGN(2) {
        *(.rodata)
        *(.rodata.*)
    }

    .data : ALIGN(2) {
        *(.data)
        *(.data.*)
    }

    __image_end = .;

    .bss (NOLOAD) : ALIGN(2) {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
        __bss_size = __bss_end - __bss_start;
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}

