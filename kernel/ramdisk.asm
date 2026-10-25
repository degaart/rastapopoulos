section .rodata
align 16

global ramdisk_start
global ramdisk_end

ramdisk_start:
    incbin "../user/build/user.tar"
ramdisk_end:

