.SUFFIXES:
.PHONY: all clean

AS := nasm
AR := i686-pc-elf-ar
RANLIB := i686-pc-elf-ranlib
CC := clang
CPP := clang
LD := i686-pc-elf-ld
OBJCOPY := i686-pc-elf-objcopy
BASEDIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

CPPFLAGS := \
	-I$(BASEDIR)/libc/include \
	--system-header-prefix=$(BASEDIR)/libc/include \
	-masm=intel \
	-ffreestanding -fno-builtin -nostdlib


CFLAGS := \
	-std=gnu99 \
	-O0 \
	-march=i386 -target i686-pc-elf  \
	-Werror -Wfatal-errors

LDFLAGS := \
	-nostdlib \
	-static \
	-L$(BASEDIR)/libc/obj

SRCS := $(wildcard *.c)
ASM_SRCS := $(wildcard *.asm)

OBJS := $(patsubst %.c,obj/%.c.o,$(SRCS))
ASM_OBJS := $(patsubst %.asm,obj/%.asm.o,$(ASM_SRCS))
