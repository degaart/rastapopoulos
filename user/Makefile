.PHONY: all clean

AS := nasm
CC := i686-elf-gcc
OBJCOPY := i686-elf-objcopy
CFLAGS := -DRASTA -DRASTA_USER \
		  -ffreestanding -fno-pic -fno-asynchronous-unwind-tables \
		  -fdata-sections -ffunction-sections \
		  -march=i386 -O0 -g \
		  -mincoming-stack-boundary=2 -mpreferred-stack-boundary=2 \
		  -fno-strict-aliasing \
		  -Wall -Wno-unused-variable -Wno-unused-but-set-variable
LDFLAGS := -Tuser.ld \
		   -Wl,-Map=build/user.map \
		   -Wl,--no-warn-rwx-segments \
		   -ffreestanding -nostdlib -g -lgcc
HDRS := $(wildcard *.h)
SRCS := $(wildcard *.c)
ASM_HDRS := $(wildcard *.inc)
ASM_SRCS := $(wildcard *.asm)
OBJS := $(addprefix build/,$(notdir $(SRCS:.c=.o))) \
		$(addprefix build/,$(notdir $(ASM_SRCS:.asm=.o)))

all: build/user.bin

build/user.bin: build/user.elf
	$(OBJCOPY) $< $@ -O binary -S -g

build/user.elf: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

build/%.o: %.asm $(ASM_HDRS) | build
	$(AS) -o $@ -f elf32 $<

build/%.o: %.c $(SRCS) | build
	$(CC) -c -o $@ $< -save-temps=obj -masm=intel $(CFLAGS)

build:
	mkdir -p build

clean:
	rm -rf build

