.PHONY: all clean

CC := i686-elf-gcc
LD := i686-elf-gcc
CFLAGS := -std=gnu99 -ffreestanding
LDFLAGS := -ffreestanding -nostdlib -lgcc

all: obj obj/kernel.elf

obj:
	mkdir -p obj

obj/kernel.elf: obj/kentry.o obj/kmain.o
	$(LD) -o $@ -T kernel.ld $^ $(LDFLAGS)

obj/kentry.o: kentry.asm
	nasm -o $@ -felf32 $^

obj/kmain.o: kmain.c
	$(CC) -c -o $@ $(CFLAGS) $^


