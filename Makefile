NASM ?= nasm
QEMU ?= qemu-system-i386

BUILD_DIR := build
BOOT_BIN := $(BUILD_DIR)/boot.bin
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
IMAGE := $(BUILD_DIR)/beeros.img

.PHONY: all run clean verify

all: $(IMAGE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BOOT_BIN): boot/boot.asm | $(BUILD_DIR)
	$(NASM) -f bin $< -o $@

$(KERNEL_BIN): kernel/kernel_entry.asm | $(BUILD_DIR)
	$(NASM) -f bin $< -o $@

$(IMAGE): $(BOOT_BIN) $(KERNEL_BIN)
	cat $(BOOT_BIN) $(KERNEL_BIN) > $(IMAGE)
	$(MAKE) verify

verify: $(BOOT_BIN) $(KERNEL_BIN) $(IMAGE)
	test $$(wc -c < $(BOOT_BIN)) -eq 512
	test $$(wc -c < $(KERNEL_BIN)) -eq 512
	test $$(wc -c < $(IMAGE)) -eq 1024

run: $(IMAGE)
	$(QEMU) -drive file=$(IMAGE),format=raw,if=floppy -boot a -no-reboot -no-shutdown

clean:
	rm -rf $(BUILD_DIR)
