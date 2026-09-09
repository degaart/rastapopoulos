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

build/bootsect.bin: bootsect.asm | build
	@nasm -fbin -o $@ $<

build/rastaldr.bin: build/rastaldr.o build/main.o| build
	@ia16-elf-gcc \
		-nostdlib \
		-T rastaldr.ld \
		-Wl,-Map=build/rastaldr.map \
		-Wl,--gc-sections \
		-o $@ $^

build/main.o: main.c | build
	@ia16-elf-gcc \
		-c \
		-o $@ \
		-ffreestanding -fno-pic -fno-pie -fno-asynchronous-unwind-tables \
		-fdata-sections -ffunction-sections \
		-march=i8086 -mcmodel=tiny -mshort-jumps \
		$<

build/rastaldr.o: rastaldr.asm | build
	@nasm -felf32 -o $@ $<

build:
	@mkdir -p $@

clean:
	@rm -rf build

run:
	@qemu-system-i386 -drive file=build/rastapopoulos.img,format=raw,if=floppy -boot a --no-reboot

