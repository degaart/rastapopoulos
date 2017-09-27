#!/bin/bash

set -eou pipefail

make --no-print-directory -C tools || exit 1
make --no-print-directory -C common || exit 1
make --no-print-directory -C kernel || exit 1
make --no-print-directory -C userland || exit 1

if ! [ -f disk.img ]; then
    # Create disk image
    ./mkimage.sh disk.img RASTA 8m || {
        echo "mkimage failed"
        exit 1
    }
fi

# Copy grub config
./copyfile.sh disk.img grub.cfg L:/boot/grub || {
    echo "copyfile failed"
    exit 1
}

# create initrd
tar -cf initrd.tar -C userland/init/obj init.elf
tar -uf initrd.tar -C userland/logger/obj logger.elf
tar -uf initrd.tar -C userland/vfs/obj vfs.elf
tar -uf initrd.tar -C userland/blk/obj blk.elf
tar -uf initrd.tar -C userland/init init.c

# Copy relevant kernel files
for file in kernel/obj/kernel.elf \
            initrd.tar \
            userland/init/init.c
do
    ./copyfile.sh disk.img "$file" L:/ || {
        echo "copyfile failed"
        exit 1
    }
done




