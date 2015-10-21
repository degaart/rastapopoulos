.SUFFIXES:
.PHONY: all clean

AS=nasm
CC=clang
CPP=clang
LD=i586-elf-ld
OBJCOPY=i586-elf-objcopy

CFLAGS = \
	-std=gnu99 -Os \
	-masm=intel \
	-march=i386 -target i586-elf-unknown -ffreestanding -fno-builtin -nostdlib \
	-Werror -Wfatal-errors

LDFLAGS = \
	-nostdlib \
	-static

SRCS = hello.c
ASM_SRCS = startup.asm syscall.asm

OBJS = $(patsubst %.c,obj/%.o,$(SRCS))
ASM_OBJS = $(patsubst %.asm,obj/%.ao,$(ASM_SRCS))

all: obj/hello.bin

obj/hello.bin: obj/hello.elf
	@echo "[CONV] $^"
	@$(OBJCOPY) -O binary $^ $@

obj/hello.elf: $(ASM_OBJS) $(OBJS)
	@echo "[LD] $^"
	@$(LD) \
		-T hello.ld -o $@ \
		-Map obj/hello.map \
		$(LDFLAGS) \
		$^

obj/%.o: %.c
	@echo "[CC] $<"
	@$(CC) -S -o $@.S $(CFLAGS) $<
	@$(CC) -c -o $@ $(CFLAGS) $<

obj/%.ao: %.asm
	@echo "[ASM] $<"
	@$(AS) -f elf32 -o $@ $<

clean:
	@rm -f obj/*

