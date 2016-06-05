#!/bin/bash

set -eou pipefail

pushd . > /dev/null
cd kernel
./build.sh
popd > /dev/null

if ! [ -f disk.img ]; then
    # Create disk image
    ./mkimage.sh disk.img RASTA 100m

    # Copy grub config
    ./copyfile.sh disk.img grub.cfg L:/boot/grub
fi

# Copy relevant kernel files
./copyfile.sh disk.img kernel/obj/kernel.elf L:/

# Execute qemu
qemu-system-i386 \
    -drive file=disk.img,format=raw \
    -boot c \
    -m 128 \
    -debugcon file:/tmp/rastapopoulos.log \
    -nographic -no-reboot

