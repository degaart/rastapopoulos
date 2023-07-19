AS := nasm
CC := i686-elf-gcc

ASFLAGS := -f elf32

CFLAGS := \
    -I../common \
    -std=gnu99 -ffreestanding -Werror -march=i386 \
    -masm=intel \
    -mpreferred-stack-boundary=2 -fno-omit-frame-pointer \
    -fno-delete-null-pointer-checks -fno-finite-loops -fno-strict-aliasing \
    -Wall -Wno-unused-function -Wno-unused-variable

LDFLAGS := \
    -ffreestanding -nostdlib

DEPFLAGS := -MMD -MP

