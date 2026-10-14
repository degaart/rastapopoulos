OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(_start)

KERNEL_OFFSET = 0xc0000000;
KERNEL_LOAD_BASE = 0x00100000;

/*
 * Explicit program headers to avoid segments spanning the large
 * address space
 */
PHDRS
{
    bootstrap_text PT_LOAD FLAGS(5);
    bootstrap_data PT_LOAD FLAGS(6);
    kernel_text    PT_LOAD FLAGS(5);
    kernel_data    PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = KERNEL_LOAD_BASE;
    __kernel_phys_start = .;

    __bootstrap_start = .;
    .bootstrap.text ALIGN(4096) :
    {
        KEEP(*(.multiboot))
        *(.bootstrap.text .bootstrap.text.*)
        build/bootstrap.o(.text .text.*)
    } :bootstrap_text

    .bootstrap.data ALIGN(4096) :
    {
        *(.bootstrap.data .bootstrap.data.*)
        build/bootstrap.o(.data .data.* .rodata .rodata.*)
    } :bootstrap_data

    .bootstrap.bss ALIGN(4096) (NOLOAD) :
    {
        *(.bootstrap.bss .bootstrap.bss.*)
        build/bootstrap.o(.bss .bss.*)
    } :bootstrap_data
    __bootstrap_bss_end = .;

    . = ALIGN(4096);
    __bootstrap_end = .;

    . += KERNEL_OFFSET;
    __kernel_start = __kernel_phys_start + KERNEL_OFFSET;
    __kernel_main_start = .;

    .text ALIGN(4096) : AT(ADDR(.text) - KERNEL_OFFSET)
    {
        *(.text .text.*)
    } :kernel_text

    .rodata ALIGN(4096) : AT(ADDR(.rodata) - KERNEL_OFFSET)
    {
        *(.rodata .rodata.*)
    } :kernel_text

    .data ALIGN(4096) : AT(ADDR(.data) - KERNEL_OFFSET)
    {
        *(.data .data.*)
    } :kernel_data

    .init_array ALIGN(4) : AT(ADDR(.init_array) - KERNEL_OFFSET)
    {
        __init_array_start = .;
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array))
        __init_array_end = .;
    } :kernel_data

    .bss ALIGN(4096) (NOLOAD) : AT(ADDR(.bss) - KERNEL_OFFSET)
    {
        __bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        __bss_end = .;
    } :kernel_data

    . = ALIGN(4096);
    __kernel_end = .;
    __kernel_phys_end = __kernel_end - KERNEL_OFFSET;

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame .eh_frame.*)
    }

    ASSERT(_start >= __bootstrap_start &&
           _start < __bootstrap_end,
           "Kernel entry is not in the low bootstrap segment")

    ASSERT(__kernel_end <= 0xffc00000,
           "Kernel overlaps vmm recursive mapping window")
}

