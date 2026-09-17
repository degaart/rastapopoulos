.PHONY: all clean run format bochs
.SUFFIXES:

# disable removal of build/*.asm files by make
.SECONDARY:

AS := nasm
CPP := cpp
CC := ia16-elf-gcc
CFLAGS := -DRASTA -DRASTA_BOOTLOADER -DRASTA_KERNEL \
		  -ffreestanding -fno-pic -fno-pie -fno-asynchronous-unwind-tables \
		  -fdata-sections -ffunction-sections -march=i8086 -mcmodel=tiny \
		  -mshort-jumps -march=i8088
LDFLAGS := -nostdlib \
		   -T rastaldr.ld \
		   -Wl,-Map=build/rastaldr.map \
		   -Wl,--gc-sections \
		   -lgcc
OBJS := build/start.o \
		build/main.o \
		build/format.o \
		build/allocator.o \
		build/int13.o \
		build/string.o

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
	$(CC) -o $@ $^ $(LDFLAGS)

build/%.o: %.asm | build
	$(AS) -felf32 -o $@ $<

build/%.o: %.c | build
	$(CC) -c -o $@ $< -save-temps=obj -masm=intel $(CFLAGS)

build:
	mkdir -p $@

clean:
	rm -rf build

run:
	qemu-system-i386 \
		-drive file=build/rastapopoulos.img,format=raw,if=floppy \
		-boot a \
		--no-reboot

bochs:
	bochs -q -dbg_gui

format:
	clang-format -i *.c *.h

