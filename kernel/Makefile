.PHONY: all clean run format bochs 86box
.SUFFIXES:
.SECONDARY:

AS := nasm
CC := i686-elf-gcc
CFLAGS := -DRASTA -DRASTA_KERNEL -DUNIT_TESTS \
		  -ffreestanding -fno-pic -fno-asynchronous-unwind-tables \
		  -march=i386 -O2 -g \
		  -mincoming-stack-boundary=2 -mpreferred-stack-boundary=2 \
		  -fno-strict-aliasing \
		  -I../common -I../third-party \
		  -save-temps=obj -masm=intel \
		  -Wall -Wno-unused-variable -Wno-unused-but-set-variable
LDFLAGS := -Tkernel.ld \
		   -Wl,-Map=build/kernel.map \
		   -ffreestanding -nostdlib -g -lgcc

BINARY := build/kernel.elf
IMAGE := ../bootloader/build/rastapopoulos.img

HDRS := $(wildcard *.h) \
		$(wildcard ../common/*.h)
SRCS := $(wildcard *.c) \
		$(wildcard ../common/*.c) \
		test/test_vga.c
ASM_HDRS := $(wildcard *.inc) \
			$(wildcard ../common/*.inc)
ASM_SRCS := $(wildcard *.asm) \
			$(wildcard ../common/*.asm)
OBJS := $(addprefix build/,$(notdir $(SRCS:.c=.o))) \
		$(addprefix build/,$(notdir $(ASM_SRCS:.asm=.o))) \
		build/test_tar.o

all: $(IMAGE) $(BINARY)

$(IMAGE): $(BINARY)
	make -C ../bootloader

$(BINARY): $(OBJS) | build
	$(CC) -o $@ $^ $(LDFLAGS)

build/%.o: %.asm $(ASM_HDRS) | build
	$(AS) -felf32 -o $@ $<

build/%.o: ../common/%.asm $(ASM_HDRS) | build
	$(AS) -felf32 -o $@ $<

build/ramdisk.o: ../user/build/user.tar

build/vga.o: CFLAGS += -O2

build/%.o: %.c $(HDRS) $(COMMON_HDRS) | build
	$(CC) -c -o $@ $< $(CFLAGS)

build/%.o: test/%.c $(HDRS) $(COMMON_HDRS) | build
	$(CC) -c -o $@ $< $(CFLAGS)

build/%.o: ../common/%.c $(HDRS) $(COMMON_HDRS) | build
	$(CC) -c -o $@ $< $(CFLAGS)

build:
	mkdir -p $@

clean:
	rm -rf build

run: all
	qemu-system-i386 \
		-drive file=$(IMAGE),format=raw,if=floppy \
		-boot a \
		--no-reboot \
		-device isa-debug-exit,iobase=0xf4,iosize=0x04 \
		-serial stdio \
		-s

test: all
	../tools/build/wrap-test -r=3 -t=15 -- \
	qemu-system-i386 \
		-drive file=$(IMAGE),format=raw,if=floppy \
		-boot a \
		--no-reboot \
		-device isa-debug-exit,iobase=0xf4,iosize=0x04 \
		-serial stdio \
		-s


debug: all
	qemu-system-i386 \
		-drive file=$(IMAGE),format=raw,if=floppy \
		-boot a \
		--no-reboot \
		-device isa-debug-exit,iobase=0xf4,iosize=0x04 \
		-serial stdio \
		-S -s

format:
	clang-format -i $(HDRS) $(SRCS)

bochs: all
	bochs -q -dbg_gui

86box: all
	$(HOME)/Applications/86box/bin/86Box \
		-N \
		-L /dev/null \
		--vmname rasta386 \
		--vmpath "$(HOME)/.local/share/86Box/Virtual Machines/rasta386" > /dev/null 2>&1


