#!/bin/bash

set -eou pipefail

[ -d "obj" ] || mkdir -v obj

find . -name '*.asm' -exec ./compile-asm.sh '{}' ';'
find . -name '*.c' -exec ./compile.sh '{}' ';'

i686-pc-elf-ld \
    -T kernel.ld \
    -o obj/kernel.elf \
    -Map obj/kernel.map \
    -nostdlib \
    -static \
    -g $(find obj -name '*.o' -and -not -name 'kstub.asm.o')



