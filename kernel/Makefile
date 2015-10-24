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

$(OBJS):
	@echo "[CC] $<"
	@$(CXX) -S -o $@.S $(CPPFLAGS) $(CXXFLAGS) $<
	@$(CXX) -c -o $@ $(CPPFLAGS) $(CXXFLAGS) $<

obj/%.ao: %.asm
	@echo "[ASM] $<"
	@ $(AS) -f elf32 -o $@ $<

obj/Makefile.depends: $(SRCS)
	@ CPP="$(CPP)" CPPFLAGS="$(CPPFLAGS)" ./makedepend.sh $^

clean:
	@rm -f obj/*

-include obj/Makefile.depends

