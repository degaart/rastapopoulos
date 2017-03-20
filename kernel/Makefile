.SUFFIXES:
.PHONY: all clean

AS := nasm
CC := i386-pc-elf-gcc
LD := i386-pc-elf-ld
OBJCOPY := i386-pc-elf-objcopy

CFLAGS := -masm=intel \
    -ffreestanding -fno-builtin -nostdlib \
    -Werror \
    -O0 -g \
    -std=gnu99 \
    -fno-asynchronous-unwind-tables \
    -fno-strict-aliasing
LDFLAGS = -nostdlib -static -g

SRCS := $(wildcard *.c) $(wildcard test/*.c)
HDRS := $(wildcard *.h) $(wildcard test/*.h)
OBJS := $(patsubst %.c,obj/%.c.o,$(SRCS))
ASM_SRCS := $(wildcard *.asm)
ASM_OBJS := $(patsubst %.asm,obj/%.asm.o,$(ASM_SRCS))

all: obj/kernel.elf

-include obj/Depends.mk

obj/kernel.elf: $(OBJS) $(ASM_OBJS)
	@ echo "[LD] $@"
	@ $(LD) \
		-T kernel.ld \
		-o $@ \
		-Map obj/kernel.map \
		$(LDFLAGS) \
		$(OBJS) $(filter-out obj/kstub.asm.o,$(ASM_OBJS))

obj/Depends.mk:
	@ mkdir -p obj/test
	@ CC="$(CC)" \
		CFLAGS="$(CFLAGS)" \
		./makedepend.sh $(SRCS)

obj/%.c.o:
	@ mkdir -p obj/test
	@ echo "[CC] $*.c"
	@ $(CC) -c -o $@ $(CFLAGS) $*.c
	@ $(CC) -c -S -o $@.S $(CFLAGS) $*.c

obj/%.asm.o:
	@ mkdir -p obj/test
	@ echo "[AS] $*.c"
	@ $(AS) -f elf32 -o $@ $*.asm

clean:
	@ rm -r obj/*

