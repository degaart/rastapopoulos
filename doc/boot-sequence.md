# Boot sequence

## bootsect

- First-stage of bootloader
- Written in 16-bit asm
- Must be compatible with a plain 8086
- Only supports fat12
- Does not support being embedded in an MBR.
- Loaded by the BIOS at 0x7c00
- Stores BIOS boot drive number at BPB.boot_drive
- Only supports partitions where fat_size <= 10 sectors
- Load the flat binary named RASTALDR located at the root of the partition at 0x8000
- Jumps to 0x8000
- Must preserve the drive's BPB when writing it to an image

## RASTALDR

- Second-stage bootloader
- Entry point: _start in start.asm
- Uses gcc-ia16 as toolchain
- Uses the tiny model: code, data, and stack must live in the same segment
- Stack top: 0x8000
- Checks for a 386 at startup
- Loads an elf binary named KERNEL.ELF located at the root of the boot drive
- Only supports partitionless fat12 drives at the moment
- Supports only the following multiboot1 features: MULTIBOOT_INFO_MEMORY, MULTIBOOT_INFO_BOOTDEV, MULTIBOOT_INFO_BOOT_LOADER_NAME
- Enabled A20
- Enables protected mode with a flat 4Gb GDT, 0x08 = code segment, 0x10 = data segment
- Uses the BIOS to get a memory map if possible
- Uses the BIOS to get high memory info
- Does not probe for high memory
- Jumps to the kernel entry point
- At kernel entry: eax = 0x2BADB002, ebx = multiboot info

## kernel - bootstrap

- Kernel entry point: _start in start.asm
- Loaded at 0x100000 (1Mb) with paging disabled
- Sets up a new flat GDT
- Setup a temporary pagetable
- Identity-map the first 4Mb, taking gaps into account
- Maps the HIGHER_HALF identically to the first 4Mb
- Pagetable for the lower-half and the higher-half is shared
- Enables paging
- Now, 0Mb - 4Mb has the same contents as 0xc0000000 - 0xc0000000 + 4Mb
- Jumps to higher-half entry point
- Uses a bootstrap stack in the bss of the lower-half part (16kb)
- Enables serial and logs to it

## kernel - higher half start

- Higher half entry point: higher_half_start
- Runs at 0xcxxxxxx which is mapped to the first 4 megabytes of the address space
- Setups an initial_stack in the higher half bss (16kb)
- Calls kmain with multiboot signature, multiboot info address

