.SUFFIXES:
.PHONY: all clean

AS=nasm
CXX=clang++
CC=clang
CPP=clang++
LD=i586-elf-ld
OBJCOPY=i586-elf-objcopy

CPPFLAGS = \
	-O0	\
	-masm=intel \
	-march=i386 -target i586-elf-unknown -ffreestanding -fno-builtin -nostdlib \
	-Werror -Wfatal-errors \
	-g

CXXFLAGS = -std=gnu++11	-fno-exceptions -fno-rtti
	
CFLAGS = -std=gnu99 -O0

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

obj/bin2c: ../util/bin2c.c
	@echo "[CC] $<"
	@$(CC) -o $@ -Wno-format-security $<

hello.h: obj/bin2c ../hello/obj/hello.bin
	@echo "[GEN] $@"
	@obj/bin2c -o $@ ../hello/obj/hello.bin

../hello/obj/hello.bin:
	@echo "[MAKE] $@"
	@make -C ../hello

$(OBJS):
	@echo "[CC] $<"
	@$(CXX) -S -o $@.S $(CPPFLAGS) $(CXXFLAGS) $<
	@$(CXX) -c -o $@ $(CPPFLAGS) $(CXXFLAGS) $<

obj/%.ao: %.asm
	@echo "[ASM] $<"
	@ $(AS) -f elf32 -o $@ $<

obj/Makefile.depends: hello.h $(SRCS)
	@echo > obj/Makefile.depends
	@for i in $^; do \
		if [ $$i != hello.h ]; then \
			echo "[DEP] $$i"; \
			/bin/echo -n "obj/" >> obj/Makefile.depends; \
			$(CPP) -E $(CPPFLAGS) -MM $$i >> obj/Makefile.depends || rm obj/Makefile.depends ; \
		fi; \
	done


clean:
	@rm -f obj/*

-include obj/Makefile.depends

