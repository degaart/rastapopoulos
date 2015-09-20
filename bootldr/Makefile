.SUFFIXES:
.PHONY: all clean

AS=nasm
CC=clang
LD=i586-elf-ld
CPP=clang

CFLAGS = \
	-std=gnu99 -O0 -Os \
	-masm=intel \
	-m16 -march=i386 -ffreestanding -fno-builtin -nostdlib -target i586-elf-code16 \
	-Werror -Wfatal-errors

LDFLAGS = \
	-nostdlib \
	-static


SRCS = \
	bootldr.c \
	bootldr_str.c \
	disk.c \
	fat.c \
	debug.c \
	util.c \
	io.c

ASM_SRCS = \
	bootldr_stub.asm \
	inb.asm \
	outb.asm \
	bios_memmap.asm

OBJS := $(patsubst %.c,obj/%.o,$(SRCS))
ASM_OBJS := $(patsubst %.asm,obj/%.ao,$(ASM_SRCS))

all: obj/Makefile.depends obj/bootldr.bin

obj/bootldr.bin: $(ASM_OBJS) $(OBJS)
	@echo "[LD] $^"
	@ $(LD) \
		-o $@ \
		-T bootldr.ld \
		-Map obj/bootldr.map \
		$(LDFLAGS) \
		$^

$(OBJS):
	@echo "[CC] $<"
	@ $(CC) -S -o $@.S $< $(CFLAGS)
	@ $(CC) -c -o $@ $< $(CFLAGS)
	
obj/%.ao: %.asm
	@echo "[ASM] $<"
	@ $(AS) -f elf32 -o $@ $<

obj/Makefile.depends: $(SRCS)
	@echo > obj/Makefile.depends
	@for i in $^; do \
		echo "[DEP] $$i"; \
		/bin/echo -n "obj/" >> obj/Makefile.depends; \
		$(CPP) -E $(CFLAGS) -MM $$i >> obj/Makefile.depends ; \
	done

-include obj/Makefile.depends

clean:
	@rm -f obj/*
