.PHONY: all clean test format
.SUFFIXES:

AS := nasm
CC := cc
LD := ld
GCC_INCLUDE_DIR := $(shell gcc -print-file-name=include)
CFLAGS := -nostdinc -isystem "$(GCC_INCLUDE_DIR)" \
		  -ffreestanding \
		  -g \
		  -I../librastaldr -I../common \
		  -Wall -Wno-main
LDFLAGS := -nostdlib -g -T test.ld

SRCS := $(wildcard ../librastaldr/*.c) \
		$(wildcard ../common/*.c)
HDRS := $(wildcard *.h) \
		$(wildcard ../librastaldr/*.h) \
		$(wildcard ../common/*.h)
ASM_SRCS := $(wildcard ../librastaldr/*.asm) \
			$(wildcard ../common/*.asm)
ASM_HDRS := $(wildcard *.inc) \
			$(wildcard ../librastaldr/*.inc) \
			$(wildcard ../common/*.inc)
OBJS := $(addprefix build/,$(notdir $(SRCS:.c=.o))) \
		$(addprefix build/,$(notdir $(ASM_SRCS:.asm=.o)))
PROGRAMS := build/test_elf \
			build/test_fat12 \
			build/test_format

all: $(PROGRAMS)

build/%: build/%.o $(OBJS) build/rastaldr_glue.o build/start.o | build
	$(LD) -o $@ $^ $(LDFLAGS)

build/%.o: %.c $(HDRS) | build
	$(CC) -c -o $@ $< $(CFLAGS)

build/%.o: ../librastaldr/%.c $(HDRS) | build
	$(CC) -c -o $@ $< $(CFLAGS)

build/%.o: ../librastaldr/%.asm $(ASM_HDRS) | build
	$(AS) -f elf64 -o $@ $<

build/%.o: ../common/%.c $(HDRS) | build
	$(CC) -c -o $@ $< $(CFLAGS)

build/%.o: ../common/%.asm $(ASM_HDRS) | build
	$(AS) -f elf64 -o $@ $<

build/start.o: start.asm | build
	$(AS) -o $@ -f elf64 $<

test: all
	for i in $(PROGRAMS); do $$i; done

build:
	mkdir -p build

clean:
	rm -rf build

format:
	clang-format -i $(wildcard *.c) $(wildcard *.h) $(wildcard ../librastaldr/*.c) $(wildcard ../librastaldr/*.h)

