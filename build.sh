#!/bin/bash

set -eou pipefail

make --no-print-directory -C kernel || exit 1
make --no-print-directory -C userland || exit 1

if ! [ -f disk.img ]; then
    # Create disk image
    ./mkimage.sh disk.img RASTA 8m || {
        echo "mkimage failed"
        exit 1
    }

    # Copy grub config
    ./copyfile.sh disk.img grub.cfg L:/boot/grub || {
        echo "copyfile failed"
        exit 1
    }

fi

# Copy relevant kernel files
./copyfile.sh disk.img kernel/obj/kernel.elf L:/ || {
    echo "copyfile failed"
    exit 1
}

./copyfile.sh disk.img userland/obj/init.elf L:/ || {
    echo "copyfile failed"
    exit 1
}

