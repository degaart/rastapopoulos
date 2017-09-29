.SUFFIXES:
.PHONY: clean all

CC ?= cc
AR ?= ar
RANLIB ?= ranlib
CFLAGS +=
NASM ?= nasm

SRCS:=$(wildcard *.c)
HDRS:=$(wildcard *.h)
OBJS:=$(patsubst %.c,obj/%.c.o,$(SRCS))

all: obj obj/$(PROGRAM) obj/Depends.mk

clean:
	@ echo "[CLEAN] " $(wildcard obj/*)
	@ rm -f obj/*

ifneq "$(MAKECMDGOALS)" "clean"
obj/Depends.mk: $(SRCS) $(HDRS) $(ADD_SRCS)
	@ echo "[DEP] $@"
	@ CC=$(CC) CFLAGS="$(CFLAGS)" ../../common/makedepend.sh $(SRCS) $(ADD_SRCS)

-include obj/Depends.mk
endif

obj:
	@ mkdir -p obj

obj/$(PROGRAM): $(OBJS) $(ADD_OBJS)
	@ echo "[LD] $@"
	@ $(CC) -o obj/$(PROGRAM) $(LDFLAGS) $(OBJS) $(ADD_OBJS) $(LIBS)

obj/%.c.o:
	@ echo "[CC] $@"
	@ $(CC) -c -o $@ $(CFLAGS) $<


