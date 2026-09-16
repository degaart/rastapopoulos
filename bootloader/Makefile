.PHONY: all clean run format bochs
.SUFFIXES:

AS := nasm
CC := rastacc
LD := ld
OBJS := build/start.o \
		build/main.o

all: build/rastapopoulos.img

build/rastapopoulos.img: build/bootsect.bin build/rastaldr.bin | build
	if ! [ -f build/rastapopoulos.img ]; then \
		dd if=/dev/zero of=build/rastapopoulos.img bs=512 count=320 status=none; \
		mformat -i build/rastapopoulos.img -k; \
	fi
	[ -f build/bpb.bin ] || \
		dd if=build/rastapopoulos.img of=build/bpb.bin bs=1 skip=11 count=51 status=none
	dd if=build/bootsect.bin of=build/rastapopoulos.img bs=512 count=1 conv=notrunc status=none
	dd if=build/bpb.bin of=build/rastapopoulos.img bs=1 seek=11 conv=notrunc status=none
	mcopy -o -i build/rastapopoulos.img build/rastaldr.bin ::/RASTALDR.BIN
	cp build/rastapopoulos.img $(HOSTDATA)

build/bootsect.bin: bootsect.asm | build
	$(AS) -fbin -o $@ $<

build/rastaldr.bin: $(OBJS) | build
	$(LD) \
		-m elf_i386 \
		-T rastaldr.ld \
		-Map=build/rastaldr.map \
		-o $@ $^

build/start.o: start.asm | build
	$(AS) -felf32 -o $@ $<

build/%.o: build/%.asm | build
	$(AS) -felf32 -o $@ $<

build/%.asm: %.c *.h| build
	$(CC) -o $@ $<

build:
	@mkdir -p $@

clean:
	@rm -rf build

run:
	@qemu-system-i386 \
		-drive file=build/rastapopoulos.img,format=raw,if=floppy \
		-boot a \
		--no-reboot

bochs:
	@bochs -q -dbg_gui

format:
	@clang-format -i *.c *.h

