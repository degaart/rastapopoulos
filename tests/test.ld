OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

PHDRS
{
    text   PT_LOAD FILEHDR PHDRS FLAGS(5); /* R-X */
    rodata PT_LOAD             FLAGS(4); /* R-- */
    data   PT_LOAD             FLAGS(6); /* RW- */
}

SECTIONS
{
    /* Conventional Linux ET_EXEC base address. */
    . = 0x400000 + SIZEOF_HEADERS;

    .text :
    {
        *(.text)
        *(.text.*)
    } :text

    . = ALIGN(0x1000);

    .rodata :
    {
        *(.rodata)
        *(.rodata.*)

        *(.eh_frame)
        *(.eh_frame_hdr)
    } :rodata

    . = ALIGN(0x1000);

    .data :
    {
        *(.data)
        *(.data.*)
    } :data

    .bss (NOLOAD) :
    {
        __bss_start = .;

        *(.bss)
        *(.bss.*)
        *(COMMON)

        __bss_end = .;
    } :data

    /DISCARD/ :
    {
        *(.note.GNU-stack)
        *(.comment)
    }
}

