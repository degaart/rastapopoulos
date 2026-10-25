.PHONY: all clean

CARGO := cargo
OBJCOPY := i686-elf-objcopy
PROFILE ?= debug
TARGET_DIR ?= target
OUTPUT_DIR := $(TARGET_DIR)/i386-elf/$(PROFILE)

all:
	$(CARGO) build --profile $(if $(filter debug,$(PROFILE)),dev,$(PROFILE)) --target-dir $(TARGET_DIR)
	$(OBJCOPY) -O binary $(OUTPUT_DIR)/hello_rust.elf $(OUTPUT_DIR)/hello_rust.bin

clean:
	rm -rf $(TARGET_DIR)

