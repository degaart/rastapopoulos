.PHONY: all clean run format bochs
.SUFFIXES:

# disable removal of build/*.asm files by make
.SECONDARY:

AS := nasm
CPP := cpp
CC := ia16-elf-gcc
STRIP := ia16-elf-strip
OBJCOPY := ia16-elf-objcopy
IMAGE := build/rastapopoulos.img
CFLAGS := -DRASTA -DRASTA_BOOTLOADER \
		  -ffreestanding -fno-pic -fno-pie -fno-asynchronous-unwind-tables \
		  -fdata-sections -ffunction-sections -march=i8086 -mcmodel=tiny \
		  -I../librastaldr -I../common -I../third-party/freebsd \
		  -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function \
		  -g
LDFLAGS := -nostdlib \
		   -T rastaldr.ld \
		   -Wl,-Map=build/rastaldr.map \
		   -Wl,--gc-sections \
		   -Wl,--no-warn-rwx-segments \
		   -g \
		   -lgcc

SRCS := $(wildcard *.c) \
		$(wildcard ../librastaldr/*.c) \
		$(wildcard ../common/*.c)
HDRS := $(wildcard *.h) \
		$(wildcard ../librastaldr/*.h) \
		$(wildcard ../common/*.h)
ASM_SRCS := $(filter-out bootsect.asm,$(wildcard *.asm)) \
			$(wildcard ../librastaldr/*.asm) \
			$(wildcard ../common/*.asm)
ASM_HDRS := $(wildcard *.inc) \
			$(wildcard ../librastaldr/*.inc) \
			$(wildcard ../common/*.inc)

OBJS := $(addprefix build/,$(notdir $(SRCS:.c=.o))) \
		$(addprefix build/,$(notdir $(ASM_SRCS:.asm=.o)))

all: $(IMAGE)

$(IMAGE): build/bootsect.bin build/rastaldr.bin | build
	if ! [ -f $(IMAGE) ]; then \
		dd if=/dev/zero of=$(IMAGE) bs=512 count=2880 status=none; \
		mformat -i $(IMAGE) -k; \
	fi
	[ -f build/bpb.bin ] || \
		dd if=$(IMAGE) of=build/bpb.bin bs=1 skip=11 count=51 status=none
	dd if=build/bootsect.bin of=$(IMAGE) bs=512 count=1 conv=notrunc status=none
	dd if=build/bpb.bin of=$(IMAGE) bs=1 seek=11 conv=notrunc status=none
	mcopy -o -i $(IMAGE) build/rastaldr.bin ::/RASTALDR.BIN
	$(STRIP) ../kernel/build/kernel.elf -o build/kernel.stripped.elf
	mcopy -o -i $(IMAGE) build/kernel.stripped.elf ::/KERNEL.ELF
	../tools/build/calc_crc32 < build/kernel.stripped.elf > build/kernel.crc
	mcopy -o -i $(IMAGE) build/kernel.crc ::/KERNEL.CRC
	[ -n "$(HOSTDATA)" ] && cp $(IMAGE) "$(HOSTDATA)"

build/bootsect.bin: bootsect.asm $(ASM_HDRS) | build
	$(AS) -fbin -o $@ $<

build/rastaldr.bin: build/rastaldr.elf | build
	$(OBJCOPY) -O binary build/rastaldr.elf build/rastaldr.bin

build/rastaldr.elf: $(OBJS) | build
	$(CC) -o $@ $^ $(LDFLAGS)

build/%.o: %.asm $(ASM_HDRS) | build
	$(AS) -felf32 -o $@ $<

build/%.o: %.c $(HDRS) | build
	$(CC) -c -o $@ $< -save-temps=obj -masm=intel $(CFLAGS)

build/%.o: ../librastaldr/%.c $(HDRS) | build
	$(CC) -c -o $@ $< -save-temps=obj -masm=intel $(CFLAGS)

build/%.o: ../common/%.c $(HDRS) | build
	$(CC) -c -o $@ $< -save-temps=obj -masm=intel $(CFLAGS)

build:
	mkdir -p $@

clean:
	rm -rf build

run: all
	qemu-system-i386 \
		-m 128 \
		-drive file=$(IMAGE),format=raw,if=floppy \
		-boot a \
		--no-reboot

debug: all
	qemu-system-i386 \
		-drive file=$(IMAGE),format=raw,if=floppy \
		-boot a \
		--no-reboot \
		-S -s

bochs:
	bochs -q -dbg_gui

format:
	clang-format -i $(wildcard *.c) $(wildcard *.h) $(wildcard ../librastaldr/*.c) $(wildcard ../librastaldr/*.h)

