.PHONY: all clean test format
.SUFFIXES:

AS := nasm
CC := cc
LD := ld
GCC_INCLUDE_DIR := $(shell gcc -print-file-name=include)
CFLAGS := -nostdinc -isystem "$(GCC_INCLUDE_DIR)" \
		  -ffreestanding \
		  -g \
		  -I../librastaldr \
		  -Wall -Wno-main
LDFLAGS := -nostdlib -g -T test.ld
LIBRASTALDR_SRCS := $(wildcard ../librastaldr/*.c)
LIBRASTALDR_HDRS := $(wildcard ../librastaldr/*.h)
LIBRASTALDR_ASM_SRCS := $(wildcard ../librastaldr/*.asm)
LIBRASTALDR_ASM_HDRS := $(wildcard ../librastaldr/*.inc)
LIBRASTALDR_OBJS := $(addprefix build/,$(notdir $(LIBRASTALDR_SRCS:.c=.o)))
LIBRASTALDR_OBJS := $(LIBRASTALDR_OBJS) $(addprefix build/,$(notdir $(LIBRASTALDR_ASM_SRCS:.asm=.o)))
HEADERS := $(wildcard *.h *.inc) $(LIBRASTALDR_HDRS)
PROGRAMS := build/test_elf \
			build/test_fat12 \
			build/test_format

all: $(PROGRAMS)

build/%: build/%.o $(LIBRASTALDR_OBJS) build/rastaldr_glue.o build/start.o | build
	$(LD) -o $@ $^ $(LDFLAGS)

# build/test_elf: build/test_elf.o $(LIBRASTALDR_OBJS) build/rastaldr_glue.o build/start.o | build
# 	$(LD) -o $@ $^ $(LDFLAGS)

build/%.o: %.c $(HEADERS) | build
	$(CC) -c -o $@ $< $(CFLAGS)

build/%.o: ../librastaldr/%.c $(LIBRASTALDR_HDRS) | build
	$(CC) -c -o $@ $< $(CFLAGS)

build/%.o: ../librastaldr/%.asm $(LIBRASTALDDR_ASM_HDRS) | build
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

