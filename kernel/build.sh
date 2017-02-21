#!/bin/bash

set -eou pipefail

[ -d "obj" ] || mkdir -v obj

for filename in *.asm
do
    ./compile-asm.sh "$filename"
done

for filename in *.c
do
    ./compile.sh "$filename"
done

i686-pc-elf-ld \
    -T kernel.ld \
    -o obj/kernel.elf \
    -Map obj/kernel.map \
    -nostdlib \
    -static \
    -g $(find obj -name '*.o' -and -not -name 'kstub.asm.o')



