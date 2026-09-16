.PHONY: all clean test

CC := cc
CFLAGS := -Wall -fsanitize=address -fno-omit-frame-pointer -g -I../bootloader
LDFLAGS := -fsanitize=address -fno-omit-frame-pointer -g
OBJS := build/format.o \
		build/test_format.o

all: build/test_format

test: all
	build/test_format

build/test_format: $(OBJS) | build
	$(CC) -o $@ $^ $(LDFLAGS)

build/format.o: ../bootloader/format.c | build
	$(CC) -c -o $@ $< $(CFLAGS)

build/test_format.o: test_format.c | build
	$(CC) -c -o $@ $< $(CFLAGS)

build:
	mkdir -p build

clean:
	rm -rf build

