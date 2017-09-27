.SUFFIXES:
.PHONY: all clean

include ../../config.mk

CFLAGS += -I ../../common -I ../runtime

SRCS := $(wildcard *.c) $(wildcard test/*.c)
HDRS := $(wildcard *.h) $(wildcard test/*.h)
OBJS := $(patsubst %.c,obj/%.c.o,$(SRCS))

ASM_SRCS := $(wildcard *.asm)
ASM_OBJS := $(patsubst %.asm,obj/%.asm.o,$(ASM_SRCS))

all: obj obj/logger.elf

clean:
	@ rm -fr obj/*

obj:
	@ mkdir -p obj

obj/Depends.mk: $(SRCS)
	@ CC="$(CC)" \
		CFLAGS="$(CFLAGS)" \
		../../common/makedepend.sh $(SRCS)

-include obj/Depends.mk

obj/logger.elf: $(OBJS) $(ASM_OBJS) ../../common/obj/common.a ../runtime/obj/runtime.a
	@ echo "[LD] $@"
	@ $(CC) \
		-T ../userland.ld \
		-o $@ \
		-Wl,-Map,$@.map \
		$(LDFLAGS) \
		$^ \
		../runtime/obj/runtime.a \
		../../common/obj/common.a \
		-lgcc

obj/%.c.o:
	@ echo "[CC] $<"
	@ $(CC) -c -o $@ $(CFLAGS) $<
	@ $(CC) -c -S -o $@.S $(CFLAGS) $<

obj/%.asm.o:
	@ echo "[AS] $*.asm"
	@ $(AS) -f elf32 -o $@ $*.asm

