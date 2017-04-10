#!/bin/bash

set -eou pipefail

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

# Copy relevant kernel files
for file in kernel/obj/kernel.elf \
            userland/fib/obj/fib.elf \
            userland/init/obj/init.elf \
            userland/logger/obj/logger.elf \
            userland/primes/obj/primes.elf \
            userland/sleeper/obj/sleeper.elf
do
    ./copyfile.sh disk.img "$file" L:/ || {
        echo "copyfile failed"
        exit 1
    }
done


