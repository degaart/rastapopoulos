.SUFFIXES:
.PHONY: all clean

AS := nasm
CC := i386-pc-elf-gcc
LD := i386-pc-elf-ld
OBJCOPY := i386-pc-elf-objcopy

CFLAGS := -masm=intel \
    -ffreestanding -fno-builtin -nostdlib \
    -Werror \
    -O0 -g \
    -std=gnu99 \
    -fno-asynchronous-unwind-tables \
    -fno-strict-aliasing
LDFLAGS = -nostdlib -static -g

all: obj obj/init.elf

obj:
	@ mkdir -p obj

obj/init.elf: obj/stub.asm.o obj/init.c.o
	@ echo "[LD] $@"
	@ $(LD) \
		-T init.ld \
		-o $@ \
		-Map obj/init.map \
		$(LDFLAGS) \
		$^

obj/stub.asm.o: stub.asm
	@ echo "[AS] $^"
	@ $(AS) -f elf32 -o $@ $^

obj/init.c.o: init.c
	@ echo "[CC] $^"
	@ $(CC) -c -S -o $@.S $(CFLAGS) $^
	@ $(CC) -c -o $@ $(CFLAGS) $^

clean:
	@ rm -r obj/*







