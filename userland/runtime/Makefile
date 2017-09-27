.SUFFIXES:
.PHONY: all clean

include ../../config.mk

CFLAGS += -I ../../common -I .

SRCS := $(wildcard *.c) $(wildcard test/*.c)
HDRS := $(wildcard *.h) $(wildcard test/*.h)
OBJS := $(patsubst %.c,obj/%.c.o,$(SRCS))

ASM_SRCS := $(wildcard *.asm)
ASM_OBJS := $(patsubst %.asm,obj/%.asm.o,$(ASM_SRCS))

all: obj obj/runtime.a

clean:
	@ rm -rf obj/*

obj:
	@ mkdir -p obj

obj/Depends.mk: $(SRCS)
	@ CC="$(CC)" \
		CFLAGS="$(CFLAGS)" \
		../../common/makedepend.sh $(SRCS)

-include obj/Depends.mk

obj/runtime.a: $(OBJS) $(ASM_OBJS)
	@ echo "[AR] $@"
	@ $(AR) rcu $@ $^
	@ $(RANLIB) $@

obj/%.c.o:
	@ echo "[CC] $<"
	@ $(CC) -c -o $@ $(CFLAGS) $<
	@ $(CC) -c -S -o $@.S $(CFLAGS) $<

obj/%.asm.o:
	@ echo "[AS] $*.asm"
	@ $(AS) -f elf32 -o $@ $*.asm

