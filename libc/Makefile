.SUFFIXES:
.PHONY: all clean

AS=nasm
AR=i586-elf-ar
RANLIB=i586-elf-ranlib
CC=clang
CPP=clang
LD=i586-elf-ld
OBJCOPY=i586-elf-objcopy

CPPFLAGS = \
	-Iinclude \
	-std=gnu99 \
	-masm=intel \
	-ffreestanding -fno-builtin -nostdlib

CFLAGS = \
	-Os \
	-march=i386 -target i586-elf-unknown  \
	-Werror -Wfatal-errors

LDFLAGS = \
	-nostdlib \
	-static

SRCS = $(wildcard *.c)
ASM_SRCS = $(wildcard *.asm)

OBJS = $(patsubst %.c,obj/%.c.o,$(SRCS))
ASM_OBJS = $(patsubst %.asm,obj/%.asm.o,$(ASM_SRCS))

all: obj/libc.a

obj/libc.a: $(OBJS) $(ASM_OBJS)
	@ echo "[AR] $^"
	@ $(AR) rcu $@ $^
	@ $(RANLIB) $@

obj/%.c.o: %.c
	@ echo "[CC] $@"
	@ $(CC) -o $@ -c $< $(CPPFLAGS) $(CFLAGS)

obj/%.asm.o: %.asm
	@ echo "[ASM] $@"
	@ $(AS) -o $@ -f elf32 $<

clean:
	@rm -f obj/*

-include obj/Makefile.deps

