.SUFFIXES:
.PHONY: all clean

include ../../config.mk

CFLAGS += -I ../../common -I ../runtime

SRCS := $(wildcard *.c) $(wildcard test/*.c)
HDRS := $(wildcard *.h) $(wildcard test/*.h)
OBJS := $(patsubst %.c,obj/%.c.o,$(SRCS))
ASM_SRCS := $(wildcard *.asm)
ASM_OBJS := $(patsubst %.asm,obj/%.asm.o,$(ASM_SRCS))

all: obj obj/sleeper.elf

obj:
	@ mkdir -p obj

-include obj/Depends.mk

obj/sleeper.elf: $(OBJS) $(ASM_OBJS) ../../common/obj/common.a ../runtime/obj/runtime.a
	@ echo "[LD] $@"
	@ $(LD) \
		-T ../userland.ld \
		-o $@ \
		-Map $@.map \
		$(LDFLAGS) \
		$^ \
		../runtime/obj/runtime.a \
		../../common/obj/common.a

obj/Depends.mk:
	@ CC="$(CC)" \
		CFLAGS="$(CFLAGS)" \
		../../common/makedepend.sh $(SRCS)

obj/%.c.o:
	@ echo "[CC] $*.c"
	@ $(CC) -c -o $@ $(CFLAGS) $*.c
	@ $(CC) -c -S -o $@.S $(CFLAGS) $*.c

obj/%.asm.o:
	@ echo "[AS] $*.c"
	@ $(AS) -f elf32 -o $@ $*.asm

clean:
	@ rm -r obj/*

