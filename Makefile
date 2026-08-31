CROSS ?= i686-elf
NASM ?= nasm
QEMU ?= qemu-system-i386
CC := $(CROSS)-gcc
LD := $(CROSS)-ld
OBJCOPY := $(CROSS)-objcopy
CFLAGS := -m32 -std=c11 -ffreestanding -fno-builtin -fno-pic -fno-pie -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -mno-mmx -mno-sse -mno-sse2 -nostdlib -Wall -Wextra -Werror

BUILD_DIR := build
STAGE2_SECTORS ?= 8
STAGE2_BYTES := $(shell expr $(STAGE2_SECTORS) \* 512)
KERNEL_MAX_BYTES := 458752
FLOPPY_BYTES := 1474560
BOOT_BIN := $(BUILD_DIR)/boot.bin
STAGE2_BIN := $(BUILD_DIR)/stage2.bin
ENTRY_OBJ := $(BUILD_DIR)/kernel_entry.o
KERNEL_MAIN_OBJ := $(BUILD_DIR)/kernel.o
PANIC_OBJ := $(BUILD_DIR)/panic.o
SYSCALL_OBJ := $(BUILD_DIR)/syscall.o
LANGUAGE_OBJ := $(BUILD_DIR)/language.o
SPLASH_OBJ := $(BUILD_DIR)/splash.o
VGA_OBJ := $(BUILD_DIR)/vga.o
SERIAL_OBJ := $(BUILD_DIR)/serial.o
KEYBOARD_OBJ := $(BUILD_DIR)/keyboard.o
PCI_OBJ := $(BUILD_DIR)/pci.o
ATA_OBJ := $(BUILD_DIR)/ata.o
IDT_OBJ := $(BUILD_DIR)/idt.o
PIT_OBJ := $(BUILD_DIR)/pit.o
SYSTEM_OBJ := $(BUILD_DIR)/system.o
INTERRUPTS_OBJ := $(BUILD_DIR)/interrupts.o
PMM_OBJ := $(BUILD_DIR)/pmm.o
PAGING_OBJ := $(BUILD_DIR)/paging.o
HEAP_OBJ := $(BUILD_DIR)/heap.o
SCHEDULER_OBJ := $(BUILD_DIR)/scheduler.o
USER_PROCESS_OBJ := $(BUILD_DIR)/user_process.o
USER_DEMO_OBJ := $(BUILD_DIR)/user_demo.o
PROGRAMS_OBJ := $(BUILD_DIR)/programs.o
RAMFS_OBJ := $(BUILD_DIR)/ramfs.o
GAMES_OBJ := $(BUILD_DIR)/games.o
SHELL_OBJ := $(BUILD_DIR)/shell.o
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
IMAGE := $(BUILD_DIR)/efesos.img

.PHONY: all run clean verify

all: $(IMAGE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(ENTRY_OBJ): kernel/kernel_entry.asm | $(BUILD_DIR)
	$(NASM) -w+error -f elf32 $< -o $@

$(KERNEL_MAIN_OBJ): kernel/kernel.c include/ata.h include/boot_info.h cpu/idt.h cpu/tss.h games/games.h include/keyboard.h include/pci.h kernel/panic.h kernel/splash.h memory/heap.h memory/paging.h memory/pmm.h process/scheduler.h fs/ramfs.h include/serial.h shell/shell.h include/vga.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -Igames -Ikernel -Imemory -Iprocess -Ishell -c $< -o $@

$(PANIC_OBJ): kernel/panic.c kernel/panic.h include/serial.h include/vga.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Ikernel -c $< -o $@

$(SYSCALL_OBJ): kernel/syscall.c include/syscall.h cpu/idt.h cpu/pit.h process/scheduler.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -Iprocess -c $< -o $@

$(LANGUAGE_OBJ): kernel/language.c include/language.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

$(SPLASH_OBJ): kernel/splash.c kernel/splash.h shell/shell.h include/vga.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Ikernel -Ishell -c $< -o $@

$(VGA_OBJ): drivers/vga.c include/boot_info.h include/vga.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

$(SERIAL_OBJ): drivers/serial.c include/serial.h cpu/io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -c $< -o $@

$(KEYBOARD_OBJ): drivers/keyboard.c include/keyboard.h kernel/splash.h shell/shell.h include/vga.h cpu/io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -Ikernel -Ishell -c $< -o $@

$(PCI_OBJ): drivers/pci.c include/pci.h cpu/io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -c $< -o $@

$(ATA_OBJ): drivers/ata.c include/ata.h cpu/io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -c $< -o $@

$(IDT_OBJ): cpu/idt.c cpu/idt.h cpu/io.h cpu/pit.h include/keyboard.h include/serial.h include/vga.h kernel/panic.h process/scheduler.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -Ikernel -c $< -o $@

$(PIT_OBJ): cpu/pit.c cpu/pit.h cpu/io.h process/scheduler.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Icpu -Iprocess -c $< -o $@

$(SYSTEM_OBJ): cpu/system.c cpu/system.h cpu/io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Icpu -c $< -o $@

$(INTERRUPTS_OBJ): cpu/interrupts.asm | $(BUILD_DIR)
	$(NASM) -w+error -f elf32 $< -o $@

$(PMM_OBJ): memory/pmm.c memory/pmm.h include/boot_info.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Imemory -c $< -o $@

$(PAGING_OBJ): memory/paging.c memory/paging.h memory/pmm.h include/boot_info.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Imemory -c $< -o $@

$(HEAP_OBJ): memory/heap.c memory/heap.h memory/paging.h memory/pmm.h include/boot_info.h kernel/panic.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Ikernel -Imemory -c $< -o $@

$(SCHEDULER_OBJ): process/scheduler.c process/scheduler.h cpu/idt.h memory/heap.h memory/paging.h memory/pmm.h kernel/panic.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -Ikernel -Imemory -Iprocess -c $< -o $@

$(USER_PROCESS_OBJ): process/user_process.c process/user_process.h process/scheduler.h memory/paging.h memory/pmm.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Imemory -Iprocess -c $< -o $@

$(USER_DEMO_OBJ): process/user_demo.asm | $(BUILD_DIR)
	$(NASM) -w+error -f elf32 $< -o $@

$(PROGRAMS_OBJ): process/programs.c process/programs.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iprocess -c $< -o $@

$(RAMFS_OBJ): fs/ramfs.c fs/ramfs.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Ifs -c $< -o $@

$(GAMES_OBJ): games/games.c games/games.h cpu/pit.h include/vga.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -Igames -c $< -o $@

$(SHELL_OBJ): shell/shell.c shell/shell.h include/ata.h include/keyboard.h include/language.h include/pci.h include/vga.h cpu/pit.h cpu/system.h fs/ramfs.h games/games.h memory/heap.h memory/pmm.h process/programs.h process/scheduler.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -Ifs -Igames -Imemory -Iprocess -Ishell -c $< -o $@

$(KERNEL_ELF): $(ENTRY_OBJ) $(KERNEL_MAIN_OBJ) $(PANIC_OBJ) $(SYSCALL_OBJ) $(LANGUAGE_OBJ) $(SPLASH_OBJ) $(VGA_OBJ) $(SERIAL_OBJ) $(KEYBOARD_OBJ) $(PCI_OBJ) $(ATA_OBJ) $(IDT_OBJ) $(PIT_OBJ) $(SYSTEM_OBJ) $(INTERRUPTS_OBJ) $(PMM_OBJ) $(PAGING_OBJ) $(HEAP_OBJ) $(SCHEDULER_OBJ) $(USER_PROCESS_OBJ) $(USER_DEMO_OBJ) $(PROGRAMS_OBJ) $(RAMFS_OBJ) $(GAMES_OBJ) $(SHELL_OBJ) kernel/linker.ld
	$(LD) -m elf_i386 -T kernel/linker.ld -o $@ $(ENTRY_OBJ) $(KERNEL_MAIN_OBJ) $(PANIC_OBJ) $(SYSCALL_OBJ) $(LANGUAGE_OBJ) $(SPLASH_OBJ) $(VGA_OBJ) $(SERIAL_OBJ) $(KEYBOARD_OBJ) $(PCI_OBJ) $(ATA_OBJ) $(IDT_OBJ) $(PIT_OBJ) $(SYSTEM_OBJ) $(INTERRUPTS_OBJ) $(PMM_OBJ) $(PAGING_OBJ) $(HEAP_OBJ) $(SCHEDULER_OBJ) $(USER_PROCESS_OBJ) $(USER_DEMO_OBJ) $(PROGRAMS_OBJ) $(RAMFS_OBJ) $(GAMES_OBJ) $(SHELL_OBJ)

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@
	test $$(wc -c < $@) -le $(KERNEL_MAX_BYTES)
	sectors=$$((($$(wc -c < $@) + 511) / 512)); truncate -s $$((sectors * 512)) $@

$(BOOT_BIN): boot/boot.asm | $(BUILD_DIR)
	$(NASM) -w+error -D STAGE2_SECTORS=$(STAGE2_SECTORS) -f bin $< -o $@

$(STAGE2_BIN): boot/stage2.asm $(KERNEL_BIN) | $(BUILD_DIR)
	sectors=$$(($$(wc -c < $(KERNEL_BIN)) / 512)); $(NASM) -w+error -D STAGE2_SECTORS=$(STAGE2_SECTORS) -D KERNEL_SECTORS=$$sectors -f bin $< -o $@

$(IMAGE): $(BOOT_BIN) $(STAGE2_BIN) $(KERNEL_BIN)
	truncate -s $(FLOPPY_BYTES) $(IMAGE)
	dd if=$(BOOT_BIN) of=$(IMAGE) conv=notrunc status=none
	dd if=$(STAGE2_BIN) of=$(IMAGE) bs=512 seek=1 conv=notrunc status=none
	dd if=$(KERNEL_BIN) of=$(IMAGE) bs=512 seek=$$((1 + $(STAGE2_SECTORS))) conv=notrunc status=none
	$(MAKE) verify

verify: $(BOOT_BIN) $(STAGE2_BIN) $(KERNEL_BIN) $(IMAGE)
	test $$(wc -c < $(BOOT_BIN)) -eq 512
	test $$(wc -c < $(STAGE2_BIN)) -eq $(STAGE2_BYTES)
	test $$(($$(wc -c < $(KERNEL_BIN)) % 512)) -eq 0
	test $$(wc -c < $(IMAGE)) -eq $(FLOPPY_BYTES)

run: $(IMAGE)
	$(QEMU) -vga std -drive file=$(IMAGE),format=raw,if=floppy -boot a -no-reboot -no-shutdown

clean:
	rm -rf $(BUILD_DIR)
