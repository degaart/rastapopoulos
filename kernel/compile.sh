#!/bin/bash
set -eou pipefail

echo "[CC] $1"

# -fno-asynchronous-unwind-tables
CFLAGS="-masm=intel \
    -march=i386 -target i686-pc-elf -ffreestanding -fno-builtin -nostdlib \
    -Werror \
    -g \
    -std=gnu99 \
    -fno-asynchronous-unwind-tables \
    -fno-strict-aliasing"

clang \
    -c -o "obj/$(basename "$1")".o \
    ${CFLAGS} \
    "$1"

clang \
    -S \
    -o "obj/$(basename "$1")".S \
    ${CFLAGS} \
    "$1"
