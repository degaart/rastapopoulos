.PHONY: all clean run format bochs
.SUFFIXES:
.SECONDARY:

AS := nasm
CC := i686-elf-gcc
CFLAGS := -DRASTA -DRASTA_KERNEL \
		  -ffreestanding -fno-pic -fno-asynchronous-unwind-tables \
		  -fdata-sections -ffunction-sections \
		  -march=i386 -O0 -g \
		  -fno-strict-aliasing \
		  -I../common \
		  -Wall -Wno-unused-variable -Wno-unused-but-set-variable
LDFLAGS := -Tkernel.ld \
		   -Wl,-Map=build/kernel.map \
		   -ffreestanding -nostdlib -g

BINARY := build/kernel.elf
IMAGE := ../bootloader/build/rastapopoulos.img

HDRS := $(wildcard *.h) \
		$(wildcard ../common/*.h)
SRCS := $(wildcard *.c) \
		$(wildcard ../common/*.c)
ASM_HDRS := $(wildcard *.inc) \
			$(wildcard ../common/*.inc)
ASM_SRCS := $(wildcard *.asm) \
			$(wildcard ../common/*.asm)

OBJS := $(addprefix build/,$(notdir $(SRCS:.c=.o))) \
		$(addprefix build/,$(notdir $(ASM_SRCS:.asm=.o)))

all: $(IMAGE) $(BINARY)

$(IMAGE): $(BINARY)
	[ -f $(IMAGE) ] || make -C ../bootloader
	mcopy -o -i $(IMAGE) $(BINARY) ::KERNEL.ELF
	../tools/build/calc_crc32 < $(BINARY) > build/kernel.crc
	mcopy -o -i $(IMAGE) build/kernel.crc ::/KERNEL.CRC

$(BINARY): $(OBJS) | build
	$(CC) -o $@ $^ $(LDFLAGS)

build/%.o: %.asm $(ASM_HDRS) | build
	$(AS) -felf32 -o $@ $<

build/%.o: %.c $(HDRS) $(COMMON_HDRS) | build
	$(CC) -c -o $@ $< -save-temps=obj -masm=intel $(CFLAGS)

build/%.o: ../common/%.c $(HDRS) $(COMMON_HDRS) | build
	$(CC) -c -o $@ $< -save-temps=obj -masm=intel $(CFLAGS)

build:
	mkdir -p $@

clean:
	rm -rf build

run: all
	qemu-system-i386 \
		-drive file=$(IMAGE),format=raw,if=floppy \
		-boot a \
		--no-reboot \
		-device isa-debug-exit,iobase=0xf4,iosize=0x04

debug: all
	qemu-system-i386 \
		-drive file=$(IMAGE),format=raw,if=floppy \
		-boot a \
		--no-reboot \
		-device isa-debug-exit,iobase=0xf4,iosize=0x04 \
		-S -s

format:
	clang-format -i $(HDRS) $(SRCS)

