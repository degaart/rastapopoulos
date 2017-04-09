AS := nasm
CC := i386-pc-elf-gcc
LD := i386-pc-elf-ld
AR := i386-pc-elf-ar
RANLIB := i386-pc-elf-ranlib
OBJCOPY := i386-pc-elf-objcopy


CFLAGS := -masm=intel \
    -ffreestanding -fno-builtin -nostdlib \
    -Werror \
    -O0 -g \
    -std=gnu99 \
    -fno-asynchronous-unwind-tables \
    -fno-strict-aliasing

LDFLAGS = -nostdlib -static -g

