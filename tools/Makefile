.PHONY: all clean
.SUFFIXES:
.SECONDARY:

AS := nasm
CC := cc
OBJCOPY := objcopy
CFLAGS := -std=gnu99 -fno-omit-frame-pointer -fsanitize=address -masm=intel
LDFLAGS := -fno-omit-frame-pointer -fsanitize=address
HDRS := $(wildcard *.h)
SRCS := $(wildcard *.c)
ASM_HDRS := $(wildcard *.inc)
ASM_SRCS := $(wildcard *.asm)
OBJS := $(addprefix build/,$(notdir $(SRCS:.c=.o))) \
		$(addprefix build/,$(notdir $(ASM_SRCS:.asm=.o)))
PROGRAMS := $(addprefix build/,$(SRCS:.c=))

all: $(PROGRAMS)

build/%: build/%.o | build
	$(CC) -o $@ $^ $(LDFLAGS)

build/%.o: %.asm $(ASM_HDRS) | build
	$(AS) -o $@ -f elf32 $<

build/%.o: %.c $(HDRS) | build
	$(CC) -c -o $@ $< $(CFLAGS)

build:
	mkdir -p build

clean:
	rm -rf build

