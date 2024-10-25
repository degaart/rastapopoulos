.PHONY: all clean
.SUFFIXES:

UNAME := $(shell uname)
CFLAGS := -g -fsanitize=address -fno-omit-frame-pointer -Wno-format-security
LDFLAGS := -g -fsanitize=address -fno-omit-frame-pointer

ifeq ($(UNAME),Linux)
	LDFLAGS += -lbsd
endif

all: obj/generate_printf_tests obj/generate_keymap

obj:
	mkdir -p obj

obj/generate_printf_tests: generate_printf_tests.c | obj
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $<

obj/generate_keymap: generate_keymap.c | obj
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $<

clean:
	rm -rf obj

format:
	@for file in *.c *.h; do \
		[ -f "$$file" ] && \
		[ $$(wc -c < "$$file") -lt 65536 ] && { \
			echo "[FMT] $$file"; \
			clang-format -i "$$file"; \
		}; \
		true; \
	done

test:
	@echo $(LDFLAGS)
