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

all: build/user.bin

build/user.bin: build/user.elf
	$(OBJCOPY) $< $@ -O binary -S -g

build/user.elf: build/start.o build/main.o build/syscall.o
	$(CC) -o $@ $^ $(LDFLAGS)

build/%.o: %.asm | build
	$(AS) -o $@ -f elf32 $<

build/%.o: %.c | build
	$(CC) -c -o $@ $< -save-temps=obj -masm=intel $(CFLAGS)

build:
	mkdir -p build

clean:
	rm -rf build

