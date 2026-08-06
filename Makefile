CROSS ?= i686-elf
NASM ?= nasm
QEMU ?= qemu-system-i386
CC := $(CROSS)-gcc
LD := $(CROSS)-ld
OBJCOPY := $(CROSS)-objcopy

BUILD_DIR := build
KERNEL_SECTORS ?= 34
KERNEL_BYTES := $(shell expr $(KERNEL_SECTORS) \* 512)
FLOPPY_BYTES := 1474560
BOOT_BIN := $(BUILD_DIR)/boot.bin
ENTRY_OBJ := $(BUILD_DIR)/kernel_entry.o
KERNEL_MAIN_OBJ := $(BUILD_DIR)/kernel.o
LANGUAGE_OBJ := $(BUILD_DIR)/language.o
VGA_OBJ := $(BUILD_DIR)/vga.o
KEYBOARD_OBJ := $(BUILD_DIR)/keyboard.o
IDT_OBJ := $(BUILD_DIR)/idt.o
PIT_OBJ := $(BUILD_DIR)/pit.o
SYSTEM_OBJ := $(BUILD_DIR)/system.o
INTERRUPTS_OBJ := $(BUILD_DIR)/interrupts.o
PMM_OBJ := $(BUILD_DIR)/pmm.o
PAGING_OBJ := $(BUILD_DIR)/paging.o
SCHEDULER_OBJ := $(BUILD_DIR)/scheduler.o
PROGRAMS_OBJ := $(BUILD_DIR)/programs.o
RAMFS_OBJ := $(BUILD_DIR)/ramfs.o
GAMES_OBJ := $(BUILD_DIR)/games.o
SHELL_OBJ := $(BUILD_DIR)/shell.o
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
IMAGE := $(BUILD_DIR)/beeros.img

.PHONY: all run clean verify

all: $(IMAGE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(ENTRY_OBJ): kernel/kernel_entry.asm | $(BUILD_DIR)
	$(NASM) -f elf32 $< -o $@

$(KERNEL_MAIN_OBJ): kernel/kernel.c cpu/idt.h games/games.h include/keyboard.h memory/paging.h memory/pmm.h process/scheduler.h shell/shell.h include/vga.h | $(BUILD_DIR)
	$(CC) -m32 -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra -Iinclude -Icpu -Igames -Imemory -Iprocess -Ishell -c $< -o $@

$(LANGUAGE_OBJ): kernel/language.c include/language.h | $(BUILD_DIR)
	$(CC) -m32 -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra -Iinclude -c $< -o $@

$(VGA_OBJ): drivers/vga.c include/vga.h | $(BUILD_DIR)
	$(CC) -m32 -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra -Iinclude -c $< -o $@

$(KEYBOARD_OBJ): drivers/keyboard.c include/keyboard.h shell/shell.h include/vga.h cpu/io.h | $(BUILD_DIR)
	$(CC) -m32 -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra -Iinclude -Icpu -Ishell -c $< -o $@

$(IDT_OBJ): cpu/idt.c cpu/idt.h include/vga.h | $(BUILD_DIR)
	$(CC) -m32 -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra -Iinclude -Icpu -c $< -o $@

$(PIT_OBJ): cpu/pit.c cpu/pit.h cpu/io.h process/scheduler.h | $(BUILD_DIR)
	$(CC) -m32 -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra -Icpu -Iprocess -c $< -o $@

$(SYSTEM_OBJ): cpu/system.c cpu/system.h cpu/io.h | $(BUILD_DIR)
	$(CC) -m32 -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra -Icpu -c $< -o $@

$(INTERRUPTS_OBJ): cpu/interrupts.asm | $(BUILD_DIR)
	$(NASM) -f elf32 $< -o $@

$(PMM_OBJ): memory/pmm.c memory/pmm.h | $(BUILD_DIR)
	$(CC) -m32 -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra -Imemory -c $< -o $@

$(PAGING_OBJ): memory/paging.c memory/paging.h memory/pmm.h | $(BUILD_DIR)
	$(CC) -m32 -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra -Imemory -c $< -o $@

$(SCHEDULER_OBJ): process/scheduler.c process/scheduler.h | $(BUILD_DIR)
	$(CC) -m32 -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra -Iprocess -c $< -o $@

$(PROGRAMS_OBJ): process/programs.c process/programs.h | $(BUILD_DIR)
	$(CC) -m32 -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra -Iprocess -c $< -o $@

$(RAMFS_OBJ): fs/ramfs.c fs/ramfs.h | $(BUILD_DIR)
	$(CC) -m32 -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra -Ifs -c $< -o $@

$(GAMES_OBJ): games/games.c games/games.h cpu/pit.h include/vga.h | $(BUILD_DIR)
	$(CC) -m32 -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra -Iinclude -Icpu -Igames -c $< -o $@

$(SHELL_OBJ): shell/shell.c shell/shell.h include/keyboard.h include/language.h include/vga.h cpu/pit.h cpu/system.h fs/ramfs.h games/games.h process/programs.h process/scheduler.h | $(BUILD_DIR)
	$(CC) -m32 -std=c11 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -nostdlib -nostartfiles -nodefaultlibs -Wall -Wextra -Iinclude -Icpu -Ifs -Igames -Iprocess -Ishell -c $< -o $@

$(KERNEL_ELF): $(ENTRY_OBJ) $(KERNEL_MAIN_OBJ) $(LANGUAGE_OBJ) $(VGA_OBJ) $(KEYBOARD_OBJ) $(IDT_OBJ) $(PIT_OBJ) $(SYSTEM_OBJ) $(INTERRUPTS_OBJ) $(PMM_OBJ) $(PAGING_OBJ) $(SCHEDULER_OBJ) $(PROGRAMS_OBJ) $(RAMFS_OBJ) $(GAMES_OBJ) $(SHELL_OBJ) kernel/linker.ld
	$(LD) -m elf_i386 -T kernel/linker.ld -o $@ $(ENTRY_OBJ) $(KERNEL_MAIN_OBJ) $(LANGUAGE_OBJ) $(VGA_OBJ) $(KEYBOARD_OBJ) $(IDT_OBJ) $(PIT_OBJ) $(SYSTEM_OBJ) $(INTERRUPTS_OBJ) $(PMM_OBJ) $(PAGING_OBJ) $(SCHEDULER_OBJ) $(PROGRAMS_OBJ) $(RAMFS_OBJ) $(GAMES_OBJ) $(SHELL_OBJ)

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@
	test $$(wc -c < $@) -le $(KERNEL_BYTES)
	truncate -s $(KERNEL_BYTES) $@

$(BOOT_BIN): boot/boot.asm $(KERNEL_BIN) | $(BUILD_DIR)
	$(NASM) -D KERNEL_SECTORS=$(KERNEL_SECTORS) -f bin $< -o $@

$(IMAGE): $(BOOT_BIN) $(KERNEL_BIN)
	truncate -s $(FLOPPY_BYTES) $(IMAGE)
	dd if=$(BOOT_BIN) of=$(IMAGE) conv=notrunc status=none
	dd if=$(KERNEL_BIN) of=$(IMAGE) bs=512 seek=1 conv=notrunc status=none
	$(MAKE) verify

verify: $(BOOT_BIN) $(KERNEL_BIN) $(IMAGE)
	test $$(wc -c < $(BOOT_BIN)) -eq 512
	test $$(wc -c < $(KERNEL_BIN)) -eq $(KERNEL_BYTES)
	test $$(wc -c < $(IMAGE)) -eq $(FLOPPY_BYTES)

run: $(IMAGE)
	$(QEMU) -vga std -drive file=$(IMAGE),format=raw,if=floppy -boot a -no-reboot -no-shutdown

clean:
	rm -rf $(BUILD_DIR)
