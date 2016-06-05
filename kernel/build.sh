#!/bin/bash

set -eou pipefail

[ -d "obj" ] || mkdir -v obj

find . -name *.asm|while read filename
do
    ./compile-asm.sh "$filename"
done

find . -name *.c|while read filename
do
    ./compile.sh "$filename"
done

i686-pc-elf-ld \
    -T kernel.ld \
    -o obj/kernel.elf \
    -Map obj/kernel.map \
    -nostdlib \
    -static \
    -g obj/*.o



