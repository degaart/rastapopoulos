.PHONY: all clean depend

CC := i686-elf-gcc
LD := i686-elf-gcc
CFLAGS := -DPROTOTYPEOS -std=gnu99 -ffreestanding -fno-builtin -fno-strict-aliasing -Werror -Wall \
	-Wno-unused-const-variable
LDFLAGS := -ffreestanding -nostdlib -lgcc

all: obj depend obj/kernel.elf

obj:
	mkdir -p obj

depend:
	@../makedepend.sh *.c

-include obj/Depends.mk

obj/kernel.elf: obj/kentry.o obj/kmain.o
	$(LD) -o $@ -T kernel.ld $^ $(LDFLAGS)

obj/kentry.o: kentry.asm
	nasm -o $@ -felf32 $^

obj/kmain.o: kmain.c
	$(CC) -c -o $@ $(CFLAGS) $^

clean:
	rm -rf obj



