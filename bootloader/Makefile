.PHONY: all clean run debug disasm
.SUFFIXES:

include ../common/common.mk

HOST_CC := cc
ASFLAGS16 := -f bin
CFLAGS := $(CFLAGS)
LDFLAGS := $(LDFLAGS) -T boot1.ld
HOST_CFLAGS := -g -fno-omit-frame-pointer -fsanitize=address
HOST_LDFLAGS :=

SRCS := \
	boot1.c

COMMON_SRCS := \
	../common/crc32.c \
	../common/serial.c \
	../common/string.c \
	../common/vga.c

all: obj/boot.img

obj:
	mkdir -p obj obj/common

obj/boot.img: obj/boot0.bin obj/boot1.bin obj/make_bootsect ../kernel/obj/kernel.elf
	mformat -C -f 1440 -i $@
	mcopy -i $@ -Do obj/boot0.bin obj/boot1.bin ../kernel/obj/kernel.elf ::/
	./obj/make_bootsect obj/boot0.bin $@

obj/boot0.bin: boot0.asm | obj
	$(AS) -o $@ $(ASFLAGS16) $<

obj/make_bootsect: make_bootsect.c | obj
	$(HOST_CC) -o $@ $(HOST_CFLAGS) $(HOST_LDFLAGS) $<

obj/fatread: fatread.c | obj
	$(HOST_CC) -o $@ $(HOST_CFLAGS) $(HOST_LDFLAGS) $<

# boot1.o must be the first dependency
obj/boot1.bin: obj/boot1.o $(addprefix obj/,$(addsuffix .o,$(SRCS))) $(addprefix obj/common/,$(addsuffix .o,$(notdir $(COMMON_SRCS))))
	$(CC) -o $@ $(LDFLAGS) $^

obj/boot1.o: boot1.asm | obj
	$(AS) -o $@ $(ASFLAGS) $<

obj/%.c.o: %.c | obj
	$(CC) -c -o $@ $(CFLAGS) $(DEPFLAGS) -MF $(basename $@).d -save-temps $<

obj/common/%.c.o: ../common/%.c | obj
	$(CC) -c -o $@ $(CFLAGS) $(DEPFLAGS) -MF $(basename $@).d -save-temps $<

obj/crc32: crc32.c | obj
	$(HOST_CC) -o $@ $(HOST_CFLAGS) $(HOST_LDFLAGS) $<

obj/t001: t001.c | obj
	$(HOST_CC) -o $@ $(HOST_CFLAGS) $(HOST_LDFLAGS) $<

obj/t002: t002.c | obj
	$(HOST_CC) -o $@ $(HOST_CFLAGS) $(HOST_LDFLAGS) $<

DEPFILES := $(addprefix obj/,$(notdir $(SRCS:%.c=%.c.d)))
DEPFILES := $(DEPFILES) $(addprefix obj/common/,$(notdir $(COMMON_SRCS:%.c=%.c.d)))
include $(wildcard $(DEPFILES))

debug_make:
	@echo $(DEPFILES)
	@echo $(wildcard $(DEPFILES))
	@echo $(addprefix obj/,$(addsuffix .o,$(SRCS))) $(addprefix obj/common/,$(addsuffix .o,$(notdir $(COMMON_SRCS))))

clean:
	rm -rf obj

disasm: obj/boot0.bin
	ndisasm -b16 -k 3,59 $<

run: obj/boot.img
	qemu-system-i386 -drive file=obj/boot.img,if=floppy,format=raw -boot a -no-reboot -serial stdio
	#qemu-system-i386 -drive file=obj/boot.img,if=floppy,format=raw -boot a -no-reboot -nographic

debug: obj/boot.img
	bochs -f bochsrc -rc bochsinit




