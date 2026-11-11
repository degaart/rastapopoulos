.PHONY: all test fuzz-utf8 fuzz-path clean

HOSTCC ?= gcc
HOST_CFLAGS := -std=c11 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer -fno-pie
HOST_LDFLAGS := -fsanitize=address,undefined -no-pie
SEED ?= 305419896
ITERATIONS ?= 100000

all: build/fuzz_utf8 build/fuzz_path

help:
	@echo Usage: make [fuzz-utf8] [fuzz-path] [test]

build:
	mkdir -p build

build/fuzz_%: fuzz_%.c fuzz_common.h ../kernel/vfs.c ../kernel/vfs.h | build
	$(HOSTCC) $(HOST_CFLAGS) $< ../kernel/vfs.c -o $@ $(HOST_LDFLAGS)

fuzz-utf8: build/fuzz_utf8
	ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/fuzz_utf8 $(SEED) $(ITERATIONS)

fuzz-path: build/fuzz_path
	ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build/fuzz_path $(SEED) $(ITERATIONS)

test: fuzz-utf8 fuzz-path

clean:
	rm -rf build
