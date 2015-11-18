.SUFFIXES:
.PHONY: all clean

AS=nasm
CXX=clang++
CC=clang
CPP=clang++
LD=i686-pc-elf-ld
OBJCOPY=i686-pc-elf-objcopy

CPPFLAGS = \
	-O0	\
	-masm=intel \
	-march=i386 -target i686-pc-elf -ffreestanding -fno-builtin -nostdlib \
	-Werror -Wfatal-errors \
	-g

CXXFLAGS = -std=gnu++11	-fno-exceptions -fno-rtti
	
CFLAGS = -std=gnu99

LDFLAGS = \
	-nostdlib \
	-static \
	-g

include Files.mk

OBJS := $(patsubst %.cpp,obj/%.o,$(SRCS))
C_OBJS := $(patsubst %.c,obj/%.o,$(C_SRCS))
ASM_OBJS := $(patsubst %.asm,obj/%.asm.o,$(ASM_SRCS))

all: obj/Makefile.depends obj/kernel.bin

obj/kernel.bin: obj/kernel.elf
	@ echo "[CONV] $^"
	@ $(OBJCOPY) -O binary $^ $@

obj/kernel.elf: $(ASM_OBJS) $(OBJS) $(C_OBJS)
	@ echo "[LD] $^"
	@ $(LD) \
		-T kernel.ld -o $@ \
		-Map obj/kernel.map \
		$(LDFLAGS) \
		$^

$(OBJS):
	@ echo "[CC] $<"
	@ $(CXX) -S -o $@.S $(CPPFLAGS) $(CXXFLAGS) $<
	@ $(CXX) -c -o $@ $(CPPFLAGS) $(CXXFLAGS) $<

$(C_OBJS):
	@ echo "[CC] $<"
	@ $(CC) -S -o $@.S $(CPPFLAGS) $(CFLAGS) $<
	@ $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $<

obj/%.asm.o: %.asm
	@ echo "[ASM] $<"
	@ $(AS) -f elf32 -o $@ $<

obj/Makefile.depends: $(SRCS) $(C_SRCS)
	@ CXX="$(CXX)" \
		CC="$(CC)" \
		CPPFLAGS="$(CPPFLAGS)" \
		CXXFLAGS="$(CXXFLAGS)" \
		CFLAGS="$(CFLAGS)" \
		./makedepend.sh $^

clean:
	@ rm -f obj/*

-include obj/Makefile.depends

