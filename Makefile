CROSS ?= i686-elf
NASM ?= nasm
QEMU ?= qemu-system-i386
CC := $(CROSS)-gcc
LD := $(CROSS)-ld
OBJCOPY := $(CROSS)-objcopy

BUILD_DIR := build
KERNEL_SECTORS ?= 4
KERNEL_BYTES := $(shell expr $(KERNEL_SECTORS) \* 512)
BOOT_BIN := $(BUILD_DIR)/boot.bin
ENTRY_OBJ := $(BUILD_DIR)/kernel_entry.o
VGA_OBJ := $(BUILD_DIR)/vga.o
KEYBOARD_OBJ := $(BUILD_DIR)/keyboard.o
IDT_OBJ := $(BUILD_DIR)/idt.o
INTERRUPTS_OBJ := $(BUILD_DIR)/interrupts.o
PMM_OBJ := $(BUILD_DIR)/pmm.o
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
IMAGE := $(BUILD_DIR)/beeros.img

.PHONY: all run clean verify

all: $(IMAGE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(ENTRY_OBJ): kernel/kernel_entry.asm | $(BUILD_DIR)
	$(NASM) -f elf32 $< -o $@

$(VGA_OBJ): drivers/vga.c include/vga.h | $(BUILD_DIR)
	$(CC) -m32 -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra -Iinclude -c $< -o $@

$(KEYBOARD_OBJ): drivers/keyboard.c include/keyboard.h include/vga.h cpu/io.h | $(BUILD_DIR)
	$(CC) -m32 -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra -Iinclude -Icpu -c $< -o $@

$(IDT_OBJ): cpu/idt.c cpu/idt.h include/vga.h | $(BUILD_DIR)
	$(CC) -m32 -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra -Iinclude -Icpu -c $< -o $@

$(INTERRUPTS_OBJ): cpu/interrupts.asm | $(BUILD_DIR)
	$(NASM) -f elf32 $< -o $@

$(PMM_OBJ): memory/pmm.c memory/pmm.h | $(BUILD_DIR)
	$(CC) -m32 -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra -Imemory -c $< -o $@

$(KERNEL_ELF): $(ENTRY_OBJ) $(VGA_OBJ) $(KEYBOARD_OBJ) $(IDT_OBJ) $(INTERRUPTS_OBJ) $(PMM_OBJ) kernel/linker.ld
	$(LD) -m elf_i386 -T kernel/linker.ld -o $@ $(ENTRY_OBJ) $(VGA_OBJ) $(KEYBOARD_OBJ) $(IDT_OBJ) $(INTERRUPTS_OBJ) $(PMM_OBJ)

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@
	test $$(wc -c < $@) -le $(KERNEL_BYTES)
	truncate -s $(KERNEL_BYTES) $@

$(BOOT_BIN): boot/boot.asm $(KERNEL_BIN) | $(BUILD_DIR)
	$(NASM) -D KERNEL_SECTORS=$(KERNEL_SECTORS) -f bin $< -o $@

$(IMAGE): $(BOOT_BIN) $(KERNEL_BIN)
	cat $(BOOT_BIN) $(KERNEL_BIN) > $(IMAGE)
	$(MAKE) verify

verify: $(BOOT_BIN) $(KERNEL_BIN) $(IMAGE)
	test $$(wc -c < $(BOOT_BIN)) -eq 512
	test $$(wc -c < $(KERNEL_BIN)) -eq $(KERNEL_BYTES)
	test $$(wc -c < $(IMAGE)) -eq $$(expr 512 + $(KERNEL_BYTES))

run: $(IMAGE)
	$(QEMU) -drive file=$(IMAGE),format=raw,if=floppy -boot a -no-reboot -no-shutdown

clean:
	rm -rf $(BUILD_DIR)
