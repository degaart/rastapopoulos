.PHONY: all clean run
.SUFFIXES:

AS := nasm
CC := i686-elf-gcc

all: build/rastapopoulos.img

build/rastapopoulos.img: build/bootsect.bin build/rastaldr.bin
	@if ! [ -f build/rastapopoulos.img ]; then \
		dd if=/dev/zero of=build/rastapopoulos.img bs=512 count=2880 status=none; \
		mformat -i build/rastapopoulos.img -k; \
	fi
	@dd if=build/bootsect.bin of=build/rastapopoulos.img bs=512 count=1 conv=notrunc status=none
	@mcopy -i build/rastapopoulos.img -o build/rastaldr.bin ::/rastaldr.bin
	@cp build/rastapopoulos.img $(HOSTDATA)

build/%.bin: %.asm | build
	@nasm -fbin -o $@ $<

build:
	@mkdir -p $@

clean:
	@rm -rf build

run:
	@qemu-system-i386 -drive file=build/rastapopoulos.img,format=raw,if=floppy -boot a --no-reboot

