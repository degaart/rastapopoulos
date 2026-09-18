.PHONY: all clean
.SUFFIXES:
.SECONDARY:

AS := nasm
CC := i686-elf-gcc
CFLAGS := -DRASTA -DRASTA_BOOTLOADER \
		  -ffreestanding -Wall -g -O0 -fno-omit-frame-pointer \
		  -march=i386
LDFLAGS := -Tkernel.ld \
		   -Wl,-Map=build/kernel.map \
		   -ffreestanding -nostdlib -g
HEADERS := $(wildcard *.h)
OBJS := build/start.o \
		build/kmain.o

all: build/kernel.elf

build/kernel.elf: $(OBJS) | build
	$(CC) -o $@ $^ $(LDFLAGS)

build/%.o: %.asm | build
	$(AS) -felf32 -o $@ $<

build/%.o: %.c $(HEADERS) | build
	$(CC) -c -o $@ $< -save-temps=obj -masm=intel $(CFLAGS)

build:
	mkdir -p $@

clean:
	rm -rf build


