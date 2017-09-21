.SUFFIXES:
.PHONY: clean all

CC=cc

SRCS:=$(wildcard *.c)
HDRS:=$(wildcard *.h)
OBJS:=$(patsubst %.c,obj/%.c.o,$(SRCS))

all: obj obj/$(PROGRAM) obj/Depends.mk

-include obj/Depends.mk

obj:
	@ mkdir -p obj

obj/Depends.mk: $(SRCS) $(HDRS) $(ADD_SRCS)
	@ echo "[DEP] $@"
	@ CC=$(CC) CFLAGS="$(CFLAGS)" ../../common/makedepend.sh $(SRCS) $(ADD_SRCS)

obj/$(PROGRAM): $(OBJS) $(ADD_OBJS)
	@ echo "[LD] $@"
	@ $(CC) -o obj/$(PROGRAM) $(LDFLAGS) $(OBJS) $(ADD_OBJS)

obj/%.c.o:
	@ echo "[CC] $@"
	@ $(CC) -c -o $@ $(CFLAGS) $<

clean:
	@ rm -vf obj/*

