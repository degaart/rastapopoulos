.PHONY: all clean run run_graphic debug
.SUFFIXES:

include ../common/common.mk

UNIT_TEST_FLAGS := -DUNIT_TESTS
ASFLAGS := $(ASFLAGS) -g
CFLAGS := $(CFLAGS) $(UNIT_TEST_FLAGS) -g
LDFLAGS := $(LDFLAGS) -g -T kernel.ld -Xlinker -Map=obj/kernel.map

QEMU_INT_FLAGS := -d int -M smm=off
QEMU_FLAGS := -no-reboot -kernel obj/kernel.elf

SRCS := \
	gdt.c \
	idt.c \
	pic.c \
	pit.c \
	kbd.c \
	pmm.c \
	debug.c \
	vmm.c \
	kmalloc.c \
	main.c

COMMON_SRCS := \
	../common/crc32.c \
	../common/serial.c \
	../common/string.c \
	../common/vga.c \
	../common/bitset.c \
	../common/util.c

ASM_SRCS := \
	stub.asm \
	idt.asm

all: obj/kernel.elf

obj:
	mkdir -p obj obj/common

OBJS := \
	$(addprefix obj/,$(addsuffix .o,$(ASM_SRCS))) \
	$(addprefix obj/,$(addsuffix .o,$(SRCS))) \
	$(addprefix obj/common/,$(addsuffix .o,$(notdir $(COMMON_SRCS))))

DEPFILES := \
	$(addprefix obj/,$(addsuffix .d,$(SRCS))) \
	$(addprefix obj/common/,$(addsuffix .d,$(notdir $(COMMON_SRCS))))

# The $(wildcard ) yields files that exists, so we don't have file not found
# errors
include $(wildcard $(DEPFILES))	

debug_make:
	@echo "OBJS: " $(OBJS)
	@echo "DEPFILES: " $(DEPFILES)

obj/kernel.elf: $(OBJS) | obj
	$(CC) -o $@ $(LDFLAGS) $^

obj/%.asm.o: %.asm Makefile | obj
	$(AS) -o $@ $(ASFLAGS) $<

obj/%.c.o: %.c Makefile | obj
	$(CC) -c -o $@ $(CFLAGS) $(DEPFLAGS) -MF $(basename $@).d -save-temps $<

obj/common/%.c.o: ../common/%.c Makefile | obj
	$(CC) -c -o $@ $(CFLAGS) $(DEPFLAGS) -MF $(basename $@).d -save-temps $<

run: obj/kernel.elf
	# C-a x to exit qemu
	# C-a h for help
	qemu-system-i386 $(QEMU_FLAGS) -nographic

run_graphic: obj/kernel.elf
	qemu-system-i386 $(QEMU_FLAGS) -serial stdio

debug: obj/kernel.elf
	# make debug
	# gdb -tui obj/kernel.elf
	# target remote localhost:1234
	# b kmain
	# c
	qemu-system-i386 $(QEMU_FLAGS) -nographic -s -S

clean:
	rm -rf obj

