.SUFFIXES:
.PHONY: all clean

AS=nasm
CC=clang
CPP=clang
LD=i586-elf-ld

# CFLAGS = \
# 	-std=gnu99 -O0 -masm=intel -march=i386 -mno-accumulate-outgoing-args \
# 	-fverbose-asm -fno-builtin -ffreestanding -fno-inline \
# 	-Wno-pointer-to-int-cast -Werror -Wfatal-errors

CFLAGS = \
	-std=gnu99 -O0 -Os \
	-masm=intel \
	-march=i386 -target i586-elf-unknown  -ffreestanding -fno-builtin -nostdlib \
	-Werror -Wfatal-errors

LDFLAGS = \
	-nostdlib \
	-static

SRCS = \
	main.c

ASM_SRCS = \
	stub.asm \
	outb.asm \
	inb.asm

OBJS := $(patsubst %.c,obj/%.o,$(SRCS))
ASM_OBJS := $(patsubst %.asm,obj/%.ao,$(ASM_SRCS))

all: obj/Makefile.depends obj/kernel.bin

obj/kernel.bin: $(ASM_OBJS) $(OBJS) 
	@echo "[LD] $^"
	@$(LD) \
		-T kernel.ld -o $@ \
		-Map obj/kernel.map \
		$(LDFLAGS) \
		$^

$(OBJS):
	@echo "[CC] $<"
	@$(CC) -S -o $@.S $(CFLAGS) $<
	@$(CC) -c -o $@ $(CFLAGS) $<
	
obj/%.ao: %.asm
	@echo "[ASM] $<"
	@ $(AS) -f elf32 -o $@ $<

obj/Makefile.depends: $(SRCS)
	@echo > obj/Makefile.depends
	@for i in $^; do \
		echo "[DEP] $$i"; \
		/bin/echo -n "obj/" >> obj/Makefile.depends; \
		$(CPP) -E $(CFLAGS) -MM $$i >> obj/Makefile.depends ; \
	done

-include obj/Makefile.depends

clean:
	@rm -f obj/*

