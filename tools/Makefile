.PHONY: all clean
.SUFFIXES:
.SECONDARY:

CC := cc
HEADERS := $(wildcard *.h)
CFLAGS := 
LDFLAGS := 

all: build/crc32

build/crc32: build/calc_crc32.o build/crc32.o
	$(CC) -o $@ $^ $(LDFLAGS)

build/crc32.o: ../bootloader/crc32.c
	$(CC) -c -o $@ $< $(CFLAGS)

build/%.o: %.c $(HEADERS) | build
	$(CC) -c -o $@ $< $(CFLAGS)

build:
	mkdir -p build

clean:
	rm -rf build

