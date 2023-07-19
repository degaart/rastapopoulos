.PHONY: all clean
.SUFFIXES:

CFLAGS := -g -fsanitize=address -fno-omit-frame-pointer -Wno-format-security
LDFLAGS := -g -fsanitize=address -fno-omit-frame-pointer

all: generate_printf_tests

generate_printf_tests: generate_printf_tests.c
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $<

clean:
	rm -f generate_printf_tests


