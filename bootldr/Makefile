.SUFFIXES:
.PHONY: all clean

CC=i586-elf-gcc
CPP=i586-elf-cpp
LD=i586-elf-gcc

CFLAGS = \
	-std=gnu99 -O0 -masm=intel -march=i386 -mno-accumulate-outgoing-args \
	-fverbose-asm -fno-builtin -ffreestanding -fno-inline \
	-Wno-pointer-to-int-cast

LDFLAGS = \
	-nostdlib \
	-static

SRCS = \
	bootldr.c \
	bootldr_str.c \
	disk.c \
	fat.c \

ASM_SRCS = \
	bootldr_stub.asm

OBJS := $(patsubst %.c,obj/%.o,$(SRCS))
ASM_OBJS := $(patsubst %.asm,obj/%.ao,$(ASM_SRCS))

all: obj/Makefile.depends obj/bootldr.bin

obj/bootldr.bin: $(OBJS) $(ASM_OBJS)
	@echo "[LD] $^"
	@$(LD) \
		-T bootldr.ld -o $@ \
		$(LDFLAGS) \
		$^

# $@: output file
# $<: input file for current output file
# $^: all input files
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

# Dependency include file
# The hyphen avoids erros when the dependency file does not exists
-include obj/Makefile.depends

# bootldr.bin: bootldr.ld $(wildcard *.c) bootldr_stub.bin
# 	@./makeobjects.sh
# 	@echo "[LD] bootldr.bin"
# 	@i586-elf-gcc \
# 		-T bootldr.ld -o bootldr.bin \
# 		-nostdlib \
# 		-static \
# 		bootldr_stub.bin $(objects)

# bootldr_stub.bin: bootldr_stub.asm
# 	@echo "[ASM] bootldr_stub.o"
# 	@nasm -f elf32 -o bootldr_stub.bin bootldr_stub.asm > /dev/null

clean:
	@rm -f obj/*
