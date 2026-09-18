.PHONY: all clean run format bochs
.SUFFIXES:

# disable removal of build/*.asm files by make
.SECONDARY:

AS := nasm
CPP := cpp
CC := ia16-elf-gcc
STRIP := i686-elf-strip
IMAGE := build/rastapopoulos.img
CFLAGS := -DRASTA -DRASTA_BOOTLOADER \
		  -ffreestanding -fno-pic -fno-pie -fno-asynchronous-unwind-tables \
		  -fdata-sections -ffunction-sections -march=i8086 -mcmodel=tiny \
		  -mshort-jumps -march=i8088 \
		  -Wall -Wno-unused-variable
LDFLAGS := -nostdlib \
		   -T rastaldr.ld \
		   -Wl,-Map=build/rastaldr.map \
		   -Wl,--gc-sections \
		   -lgcc
HEADERS := $(wildcard *.h *.inc)
OBJS := build/start.o \
		build/main.o \
		build/format.o \
		build/allocator.o \
		build/int13.o \
		build/string.o

all: $(IMAGE)

$(IMAGE): build/bootsect.bin build/rastaldr.bin | build
	if ! [ -f $(IMAGE) ]; then \
		dd if=/dev/zero of=$(IMAGE) bs=512 count=320 status=none; \
		mformat -i $(IMAGE) -k; \
	fi
	[ -f build/bpb.bin ] || \
		dd if=$(IMAGE) of=build/bpb.bin bs=1 skip=11 count=51 status=none
	dd if=build/bootsect.bin of=$(IMAGE) bs=512 count=1 conv=notrunc status=none
	dd if=build/bpb.bin of=$(IMAGE) bs=1 seek=11 conv=notrunc status=none
	mcopy -o -i $(IMAGE) build/rastaldr.bin ::/RASTALDR.BIN
	$(STRIP) ../kernel/build/kernel.elf -o build/kernel.stripped.elf
	mcopy -o -i $(IMAGE) build/kernel.stripped.elf ::/KERNEL.ELF
	cp $(IMAGE) $(HOSTDATA)

build/bootsect.bin: bootsect.asm $(HEADERS) | build
	$(AS) -fbin -o $@ $<

build/rastaldr.bin: $(OBJS) | build
	$(CC) -o $@ $^ $(LDFLAGS)

build/%.o: %.asm $(HEADERS) | build
	$(AS) -felf32 -o $@ $<

build/%.o: %.c $(HEADERS) | build
	$(CC) -c -o $@ $< -save-temps=obj -masm=intel $(CFLAGS)

build:
	mkdir -p $@

clean:
	rm -rf build

run:
	qemu-system-i386 \
		-drive file=$(IMAGE),format=raw,if=floppy \
		-boot a \
		--no-reboot

bochs:
	bochs -q -dbg_gui

format:
	clang-format -i *.c *.h

