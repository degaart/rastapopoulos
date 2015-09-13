.SUFFIXES:
.PHONY: all clean

CC=i586-elf-gcc
CPP=i586-elf-cpp
LD=i586-elf-ld

CFLAGS = \
	-std=gnu99 -O0 -masm=intel -march=i386 -mno-accumulate-outgoing-args \
	-fverbose-asm -fno-builtin -ffreestanding -fno-inline \
	-Wno-pointer-to-int-cast

LDFLAGS = \
	-nostdlib \
	-static

SRCS = \
	main.c

ASM_SRCS = \
	kernel_stub.asm

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
	@$(CC) -c -o $@ $(CFLAGS) $<
	
$(ASM_OBJS): $(ASM_SRCS)
	@echo "[ASM] $<"
	@nasm -f elf32 -o $@ $^

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

