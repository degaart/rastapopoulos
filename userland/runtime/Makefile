.SUFFIXES:
.PHONY: all clean

include ../../config.mk

CFLAGS += -I ../../common

SRCS := $(wildcard *.c) $(wildcard test/*.c)
HDRS := $(wildcard *.h) $(wildcard test/*.h)
OBJS := $(patsubst %.c,obj/%.c.o,$(SRCS))
ASM_SRCS := $(wildcard *.asm)
ASM_OBJS := $(patsubst %.asm,obj/%.asm.o,$(ASM_SRCS))

all: obj obj/runtime.a

obj:
	@ mkdir -p obj

-include obj/Depends.mk

obj/runtime.a: $(OBJS) $(ASM_OBJS)
	@ echo "[AR] $@"
	@ $(AR) rcu $@ $^
	@ $(RANLIB) $@

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
