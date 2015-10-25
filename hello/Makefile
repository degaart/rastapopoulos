.SUFFIXES:
.PHONY: all clean

AS=nasm
AR=i586-elf-ar
RANLIB=i586-elf-ranlib
CC=clang
CPP=clang
LD=i586-elf-ld
OBJCOPY=i586-elf-objcopy
PROGRAM=hello

CPPFLAGS = \
	-I../libc/include \
	-std=gnu99 \
	-masm=intel \
	-ffreestanding -fno-builtin -nostdlib

CFLAGS = \
	-O0 \
	-march=i386 -target i586-elf-unknown  \
	-Werror -Wfatal-errors

LDFLAGS = \
	-nostdlib \
	-static \
	-L ../libc/obj \
	-lc

SRCS = $(wildcard *.c)
ASM_SRCS = $(wildcard *.asm)

OBJS = $(patsubst %.c,obj/%.c.o,$(SRCS))
ASM_OBJS = $(patsubst %.asm,obj/%.asm.o,$(ASM_SRCS))

all: obj/$(PROGRAM).bin

obj/$(PROGRAM).bin: obj/$(PROGRAM).elf
	@echo "[CONV] $^"
	@$(OBJCOPY) -O binary $^ $@

obj/$(PROGRAM).elf: $(ASM_OBJS) $(OBJS)
	@echo "[LD] $^"
	@$(LD) \
		-T $(PROGRAM).ld -o $@ \
		-Map obj/$(PROGRAM).map \
		$^ \
		$(LDFLAGS)

obj/%.c.o: %.c
	@ echo "[CC] $@"
	@ $(CC) -o $@ -c $< $(CPPFLAGS) $(CFLAGS)

obj/%.asm.o: %.asm
	@ echo "[ASM] $@"
	@ $(AS) -o $@ -f elf32 $<

clean:
	@rm -f obj/*
