.PHONY: all clean
.SUFFIXES:

CFLAGS := -g -fsanitize=address -fno-omit-frame-pointer -Wno-format-security
LDFLAGS := -g -fsanitize=address -fno-omit-frame-pointer

all: obj/generate_printf_tests obj/generate_keymap

obj:
	mkdir -p obj

obj/generate_printf_tests: generate_printf_tests.c | obj
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $<

obj/generate_keymap: generate_keymap.c | obj
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $<

clean:
	rm -rf obj


