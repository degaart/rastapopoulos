AS := nasm
CC := i686-elf-gcc
LD := i686-elf-ld
AR := i686-elf-ar
RANLIB := i686-elf-ranlib
OBJCOPY := i686-elf-objcopy


CFLAGS := -masm=intel \
    -ffreestanding -fno-builtin -nostdlib \
    -Werror \
    -O0 -g \
    -std=gnu99 \
    -fno-asynchronous-unwind-tables \
    -fno-strict-aliasing \
	-DRASTAPOPOULOS

LDFLAGS = -nostdlib -static -g

