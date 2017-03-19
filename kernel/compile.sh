#!/bin/bash
set -eou pipefail

echo "[CC] $1"

# -fno-asynchronous-unwind-tables
CFLAGS="-masm=intel \
    -ffreestanding -fno-builtin -nostdlib \
    -Werror \
    -O0 -g \
    -std=gnu99 \
    -fno-asynchronous-unwind-tables \
    -fno-strict-aliasing"

i386-pc-elf-gcc \
    -c -o "obj/$(basename "$1")".o \
    -pipe \
    ${CFLAGS} \
    "$1"

i386-pc-elf-gcc \
    -S \
    -o "obj/$(basename "$1")".S \
    -pipe \
    ${CFLAGS} \
    "$1"
