.PHONY: all clean depend

CC := i686-elf-gcc
LD := i686-elf-gcc
CFLAGS := \
	-DPROTOTYPEOS \
	-std=gnu99 \
	-masm=intel \
	-ffreestanding \
	-fno-builtin \
	-fno-strict-aliasing \
	-Werror -Wall \
	-Wno-unused-const-variable \
	-Wno-unused-variable
LDFLAGS := -ffreestanding \
	-nostdlib \
	-lgcc

SRCS := $(wildcard *.c)
HDRS := $(wildcard *.h)
OBJS := $(patsubst %.c,obj/%.c.o,$(SRCS))
ASM_SRCS := $(wildcard *.asm)
ASM_OBJS := $(patsubst %.asm,obj/%.asm.o,$(ASM_SRCS))

all: obj depend obj/kernel.elf

obj:
	@echo "SRCS: $(SRCS)"
	@echo "HDRS: $(HDRS)"
	@echo "OBJS: $(OBJS)"
	@echo "ASM_SRCS: $(ASM_SRCS)"
	@echo "ASM_OBJS: $(ASM_OBJS)"
	mkdir -p obj

depend:
	@../makedepend.sh *.c

-include obj/Depends.mk

obj/kernel.elf: $(OBJS) $(ASM_OBJS)
	$(LD) -o $@ -T kernel.ld -Wl,-Map,obj/kernel.map $^ $(LDFLAGS)

obj/%.c.o:
	$(CC) -c -o $@ $(CFLAGS) $*.c

obj/%.asm.o: %.asm
	nasm -o $@ -felf32 $*.asm

clean:
	rm -rf obj



