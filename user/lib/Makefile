# vim: ft=make
.PHONY: all clean
.SUFFIXES:
.SECONDARY:

AS := nasm
CC := i686-elf-gcc
AR := i686-elf-ar
OBJCOPY := i686-elf-objcopy
CFLAGS := -DRASTA -DRASTA_USER \
		  -ffreestanding -fno-pic -fno-asynchronous-unwind-tables \
		  -march=i386 -O0 -g \
		  -mincoming-stack-boundary=2 -mpreferred-stack-boundary=2 \
		  -fno-strict-aliasing \
		  -masm=intel \
		  -I../../common -I../../third-party \
		  -Wall -Wno-unused-variable -Wno-unused-but-set-variable
LDFLAGS := -T../user.ld \
		   -Wl,-Map=$(PROGRAM:.bin=.map) \
		   -Wl,--no-warn-rwx-segments \
		   -ffreestanding -nostdlib -g -lgcc

HDRS := $(wildcard *.h) \
		$(wildcard ../../common/*.h)
SRCS := $(wildcard *.c) \
		$(wildcard ../lib/*.c) \
		$(wildcard ../../common/*.c)
ASM_HDRS := $(wildcard *.inc)
ASM_SRCS := $(filter-out start.asm,$(wildcard *.asm)) \
			$(wildcard ../../common/*.asm)
OBJS := $(addprefix build/,$(notdir $(SRCS:.c=.o))) \
		$(addprefix build/,$(notdir $(ASM_SRCS:.asm=.o)))

all: build/libcrt0.a build/librasta.a

build/libcrt0.a: build/start.o
	$(AR) rcu $@ $^

build/librasta.a: $(OBJS)
	$(AR) rcu $@ $^

build/%.o: %.asm $(ASM_HDRS) | build
	$(AS) -o $@ -f elf32 $<

build/%.o: ../../common/%.asm $(ASM_HDRS) | build
	$(AS) -o $@ -f elf32 $<

build/%.o: %.c $(HDRS) | build
	$(CC) -c -o $@ $< $(CFLAGS)

build/%.o: ../../common/%.c $(HDRS) | build
	$(CC) -c -o $@ $< $(CFLAGS)

build:
	mkdir -p build

clean:
	rm -rf build


build/rasta.o: CFLAGS += -O2
