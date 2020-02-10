.PHONY: all clean depend
.SUFFIXES:

CC := i686-elf-gcc
LD := i686-elf-gcc

CFLAGS := \
	-DPROTOTYPEOS \
	-std=gnu99 \
	-masm=intel \
	-ffreestanding \
	-fno-builtin \
	-fno-strict-aliasing \
	-fno-asynchronous-unwind-tables \
	-g \
	-Werror -Wall \
	-Wno-unused-const-variable \
	-Wno-unused-variable \
	-Wno-unused-function \
	-Wno-unused-but-set-variable
LDFLAGS := \
	-g \
	-ffreestanding \
	-nostdlib \
	-lgcc

SRCS := $(wildcard *.c)
HDRS := $(wildcard *.h)
OBJS := $(patsubst %.c,obj/%.c.o,$(SRCS))
ASM_SRCS := $(wildcard *.asm)
ASM_OBJS := $(patsubst %.asm,obj/%.asm.o,$(ASM_SRCS))

all: obj depend obj/hello.elf

obj:
	@mkdir -p obj

depend:
	@../../makedepend.sh *.c

-include obj/Depends.mk

obj/hello.elf: $(OBJS) $(ASM_OBJS)
	@echo "[LD] $@"
	@$(LD) -o $@ -T hello.ld -Wl,-Map,obj/hello.map $^ $(LDFLAGS)

obj/%.c.o:
	@echo "[CC] $*.c"
	@$(CC) -c -o $@ $(CFLAGS) $*.c
	@$(CC) -c -S -o $@.S $(CFLAGS) $*.c

obj/%.asm.o: %.asm
	@echo "[ASM] $*.asm"
	@nasm -o $@ -felf32 $*.asm

clean:
	@rm -rf obj

