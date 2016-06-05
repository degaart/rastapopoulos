#!/bin/bash
set -eou pipefail

clang \
    -c -o "obj/$1".o \
    -O0 \
    -masm=intel \
    -march=i386 -target i686-pc-elf -ffreestanding -fno-builtin -nostdlib \
    -Werror -Wfatal-errors \
    -g \
    -std=gnu99 \
    "$1"

