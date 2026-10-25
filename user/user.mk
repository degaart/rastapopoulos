# vim: ft=make
.PHONY: all clean
.SUFFIXES:
.SECONDARY:

AS := nasm
CC := i686-elf-gcc
OBJCOPY := i686-elf-objcopy
CFLAGS := -DRASTA -DRASTA_USER \
		  -ffreestanding -fno-pic -fno-asynchronous-unwind-tables \
		  -march=i386 -O0 -g \
		  -mincoming-stack-boundary=2 -mpreferred-stack-boundary=2 \
		  -fno-strict-aliasing \
		  -masm=intel \
		  -I../lib -I../../common -I../../third-party \
		  -Wall -Wno-unused-variable -Wno-unused-but-set-variable
LDFLAGS := -T../user.ld \
		   -Wl,-Map=$(PROGRAM:.bin=.map) \
		   -Wl,--no-warn-rwx-segments \
		   -L ../lib/build \
		   -ffreestanding -nostdlib -g -lcrt0 -lrasta -lgcc

HDRS := $(wildcard *.h) \
		$(wildcard ../lib/*.h) \
		$(wildcard ../../common/*.h)
SRCS := $(wildcard *.c)
ASM_HDRS := $(wildcard *.inc) $(wildcard ../lib/*.inc)
ASM_SRCS := $(wildcard *.asm)
OBJS := $(addprefix build/,$(notdir $(SRCS:.c=.o))) \
		$(addprefix build/,$(notdir $(ASM_SRCS:.asm=.o)))
ELF_FILE := $(PROGRAM:.bin=.elf)

all: $(PROGRAM)

$(PROGRAM): $(ELF_FILE)
	$(OBJCOPY) \
		--set-section-flags bss=alloc,load,contents \
		-O binary -S -g \
		$< $@

$(ELF_FILE): $(OBJS) ../lib/build/libcrt0.a ../lib/build/librasta.a
	$(CC) -o $@ $^ $(LDFLAGS)

build/%.o: %.asm $(ASM_HDRS) | build
	$(AS) -o $@ -f elf32 $<

build/%.o: %.c $(HDRS) | build
	$(CC) -c -o $@ $< $(CFLAGS)

build:
	mkdir -p build

clean:
	rm -rf build

