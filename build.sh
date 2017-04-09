#!/bin/bash

set -eou pipefail

pushd . > /dev/null
cd kernel
make || {
    echo "Build failed"
    exit 1
}
popd > /dev/null

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

./copyfile.sh disk.img bochsrc L:/ || {
    echo "copyfile failed"
    exit 1
}

./copyfile.sh disk.img userland/init L:/ || {
    echo "copyfile failed"
    exit 1
}

