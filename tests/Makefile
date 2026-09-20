.PHONY: all clean test

CC := cc
CFLAGS := -Wall -fsanitize=address -fno-omit-frame-pointer \
		  -g -I../bootloader \
		  -Wno-main
LDFLAGS := -fsanitize=address -fno-omit-frame-pointer -g
OBJS := build/format.o \
		build/test_format.o \
		build/test_fat12.o \
		build/fat12.o

all: build/test_format build/test_fat12

test: all
	build/test_format

build/test_format: build/test_format.o build/format.o | build
	$(CC) -o $@ $^ $(LDFLAGS)

build/format.o: ../bootloader/format.c | build
	$(CC) -c -o $@ $< $(CFLAGS)

build/test_format.o: test_format.c | build
	$(CC) -c -o $@ $< $(CFLAGS)

build/test_fat12: build/test_fat12.o build/fat12.o build/crc32.o | build
	$(CC) -o $@ $^ $(LDFLAGS)

build/test_fat12.o: test_fat12.c | build
	$(CC) -c -o $@ $< $(CFLAGS)

build/fat12.o: fat12.c | build
	$(CC) -c -o $@ $< $(CFLAGS)

build/crc32.o: ../bootloader/crc32.c | build
	$(CC) -c -o $@ $< $(CFLAGS)

build:
	mkdir -p build

clean:
	rm -rf build

