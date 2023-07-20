.PHONY: all clean
.SUFFIXES:

include ../common/common.mk

CFLAGS 	:= $(CFLAGS)
LDFLAGS := $(LDFLAGS) -T user.ld

SRCS := \
	program1.c \
	program2.c

COMMON_SRCS := \
	../common/crc32.c \
	../common/serial.c \
	../common/string.c \
	../common/vga.c \
	../common/bitset.c \
	../common/util.c

PROGRAMS := $(addprefix obj/,$(addsuffix .elf,$(basename $(SRCS))))

all: $(PROGRAMS)

obj:
	mkdir -p obj obj/common

COMMON_OBJS := \
	$(addprefix obj/common/,$(addsuffix .o,$(notdir $(COMMON_SRCS))))

DEPFILES := \
	$(addprefix obj/,$(addsuffix .d,$(SRCS))) \
	$(addprefix obj/common/,$(addsuffix .d,$(notdir $(COMMON_SRCS))))

# The $(wildcard ) yields files that exists, so we don't have file not found
# errors
include $(wildcard $(DEPFILES))	

debug_make:
	@echo "PROGRAMS:    $(PROGRAMS)"
	@echo "COMMON_OBJS: $(COMMON_OBJS)"
	@echo "DEPFILES:    $(DEPFILES)"

obj/%.elf: obj/%.c.o obj/user.c.o $(COMMON_OBJS) Makefile | obj
	$(CC) -o $@ $(LDFLAGS) -Xlinker -Map=$(basename $@).map $< obj/user.c.o $(COMMON_OBJS)
	xxd -i $@ > $(basename $@).h

obj/%.c.o: %.c Makefile | obj
	$(CC) -c -o $@ $(CFLAGS) $(DEPFLAGS) -MF $(basename $@).d -save-temps $<

obj/common/%.c.o: ../common/%.c Makefile | obj
	$(CC) -c -o $@ $(CFLAGS) $(DEPFLAGS) -MF $(basename $@).d -save-temps $<

clean:
	rm -rf obj

