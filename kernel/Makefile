.SUFFIXES:
.PHONY: all clean

include ../config.mk

CFLAGS += -I ../common -DKERNEL

SRCS := $(wildcard *.c) $(wildcard test/*.c)
HDRS := $(wildcard *.h) $(wildcard test/*.h)
OBJS := $(patsubst %.c,obj/%.c.o,$(SRCS))
ASM_SRCS := $(wildcard *.asm)
ASM_OBJS := $(patsubst %.asm,obj/%.asm.o,$(ASM_SRCS))

all: obj obj/test obj/kernel.elf

obj:
	@ mkdir -p obj

obj/test: obj
	@ mkdir -p obj/test

-include obj/Depends.mk

obj/kernel.elf: $(OBJS) $(ASM_OBJS) ../common/obj/common.a
	@ echo "[LD] $@"
	@ $(CC) \
		-T kernel.ld \
		-o $@ \
		-Wl,-Map,obj/kernel.map \
		$(LDFLAGS) \
		$(OBJS) \
		$(filter-out obj/kstub.asm.o,$(ASM_OBJS)) \
		../common/obj/common.a \
		-lgcc

obj/Depends.mk:
	@ CC="$(CC)" \
		CFLAGS="$(CFLAGS)" \
		../common/makedepend.sh $(SRCS)

obj/%.c.o:
	@ echo "[CC] $*.c"
	@ $(CC) -c -o $@ $(CFLAGS) $*.c
	@ $(CC) -c -S -o $@.S $(CFLAGS) $*.c

obj/%.asm.o: %.asm
	@ echo "[AS] $*.asm"
	@ $(AS) -f elf32 -o $@ $*.asm

int10_stub.h: int10.asm16
	@ echo "[AS] int10_stub.bin"
	@ $(AS) -f bin -o obj/int10_stub.bin int10.asm16
	@ echo "[BIN] $@"
	@ ../tools/bin2c/obj/bin2c -o int10_stub.h obj/int10_stub.bin

clean:
	@ rm -r obj/*

