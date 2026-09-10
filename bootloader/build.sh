#!/bin/bash
set -eou pipefail

[ -d build ] || mkdir -p build

if ! [ -f build/rasta86.img ]; then
    dd if=/dev/zero of=build/rasta86.img bs=512 count=320 status=none
    mformat -i build/rasta86.img -k
fi

if ! [ -f build/bpb.bin ]; then
    dd if=build/rasta86.img of=build/bpb.bin bs=1 skip=$((0x0B)) count=$((0x33)) status=none
fi

nasm -fbin -o build/bootsect.bin bootsect.asm
nasm -fbin -o build/rastaldr.bin rastaldr86.asm
dd if=build/bootsect.bin of=build/rasta86.img bs=512 count=1 conv=notrunc status=none
dd if=build/bpb.bin of=build/rasta86.img bs=1 seek=$((0x0B)) conv=notrunc status=none
mcopy -o -i build/rasta86.img build/rastaldr.bin ::/RASTALDR.BIN

cp build/rasta86.img ${HOSTDATA}
qemu-system-i386 \
    -drive file=build/rasta86.img,format=raw,if=floppy \
    -boot a \
    --no-reboot

