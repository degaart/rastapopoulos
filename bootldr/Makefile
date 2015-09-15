.SUFFIXES:
.PHONY: all clean

AS=nasm
CC=smlrcc
LD=i586-elf-ld
CPP=i586-elf-cpp

LDFLAGS = \
	-nostdlib \
	-static


SRCS = \
	main.c \
	term.c \
	mem.c \
	util.c \
	io.c \
    debug.c

ASM_SRCS = \
	stub.asm 

OBJS := $(patsubst %.c,obj/%.o,$(SRCS))
ASM_OBJS := $(patsubst %.asm,obj/%.ao,$(ASM_SRCS))

all: obj/Makefile.depends obj/bootldr.bin

obj/bootldr.bin: $(ASM_OBJS) $(OBJS)
	@echo "[LD] $^"
	@ $(LD) \
		-o $@ \
		-T bootldr.ld \
		-Map obj/bootldr.map \
		$(LDFLAGS) \
		$^

$(OBJS):
	@echo "[CC] $<"
	@ $(CPP) -E $< -o $@.i --std=gnu99
	@ $(CC) -c -S -flat16 $@.i
	@ $(AS) -f elf32 -o $@ $@.asm
	
obj/%.ao: %.asm
	@echo "[ASM] $<"
	@ $(AS) -f elf32 -o $@ $<

obj/Makefile.depends: $(SRCS)
	@echo > obj/Makefile.depends
	@for i in $^; do \
		echo "[DEP] $$i"; \
		/bin/echo -n "obj/" >> obj/Makefile.depends; \
		$(CPP) $(CFLAGS) -MM $$i >> obj/Makefile.depends ; \
	done

-include obj/Makefile.depends

clean:
	@rm -f obj/*
