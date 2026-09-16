#!/bin/bash
set -eou pipefail

[ -d build ] || mkdir -p build

if ! [ -f build/rasta86.img ]; then
    dd if=/dev/zero of=build/rasta86.img bs=512 count=320 status=none
    mformat -i build/rasta86.img -k
fi

if ! [ -f build/bpb.bin ]; then
    dd if=build/rasta86.img of=build/bpb.bin bs=1 skip=11 count=51 status=none
fi

nasm -fbin -o build/bootsect.bin bootsect.asm
nasm -felf32 -o build/rastaldr86.asm.o rastaldr86.asm
rastacc -o build/rastaldr86.c.asm rastaldr86.c
nasm -felf32 -o build/rastaldr86.c.asm.o build/rastaldr86.c.asm
ld \
    -m elf_i386 \
    -T rastaldr.ld \
    -o build/rastaldr.bin \
    -Map build/rastaldr.map \
    build/rastaldr86.c.asm.o build/rastaldr86.asm.o
#objcopy -O binary build/rastaldr.elf build/rastaldr.bin

dd if=build/bootsect.bin of=build/rasta86.img bs=512 count=1 conv=notrunc status=none
dd if=build/bpb.bin of=build/rasta86.img bs=1 seek=11 conv=notrunc status=none
mcopy -o -i build/rasta86.img build/rastaldr.bin ::/RASTALDR.BIN

cp build/rasta86.img ${HOSTDATA}
qemu-system-i386 \
    -drive file=build/rasta86.img,format=raw,if=floppy \
    -boot a \
    --no-reboot

