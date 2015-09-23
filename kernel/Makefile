.SUFFIXES:
.PHONY: all clean

AS=nasm
CC=clang++
CPP=clang++
LD=i586-elf-ld
OBJCOPY=i586-elf-objcopy

CFLAGS = \
	-std=gnu++11 -O0 \
	-masm=intel \
	-march=i386 -target i586-elf-unknown -ffreestanding -fno-builtin -nostdlib \
	-fno-exceptions -fno-rtti \
	-Werror -Wfatal-errors \
	-g

LDFLAGS = \
	-nostdlib \
	-static \
	-g

include Files.mk

OBJS := $(patsubst %.cpp,obj/%.o,$(SRCS))
ASM_OBJS := $(patsubst %.asm,obj/%.ao,$(ASM_SRCS))

all: obj/Makefile.depends obj/kernel.bin

obj/kernel.bin: obj/kernel.elf
	@echo "[CONV] $^"
	@$(OBJCOPY) -O binary $^ $@

obj/kernel.elf: $(ASM_OBJS) $(OBJS) 
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
		$(CPP) -E $(CFLAGS) -MM $$i >> obj/Makefile.depends || rm obj/Makefile.depends ; \
	done

-include obj/Makefile.depends

clean:
	@rm -f obj/*

