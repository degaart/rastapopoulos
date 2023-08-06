AS := nasm
CC := i686-elf-gcc

ASFLAGS := -f elf32

CFLAGS := \
    -I../common \
    -std=gnu99 -ffreestanding -Werror -march=i386 \
    -masm=intel \
    -mpreferred-stack-boundary=2 -fno-omit-frame-pointer \
    -fno-delete-null-pointer-checks -fno-finite-loops -fno-strict-aliasing \
    -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-local-typedefs

LDFLAGS := \
    -ffreestanding -nostdlib

DEPFLAGS := -MMD -MP

COMMON_SRCS := \
	crc32.c \
	serial.c \
	string.c \
	vga.c \
	bitset.c \
	util.c \
	rbuf.c

format:
	@for file in *.c *.h; do \
		[ -f "$$file" ] && \
		[ $$(wc -c < "$$file") -lt 65536 ] && { \
			echo "[FMT] $$file"; \
			clang-format -i "$$file"; \
		}; \
		true; \
	done

