CROSS ?= i686-elf
NASM ?= nasm
QEMU ?= qemu-system-i386
PYTHON ?= python3
ifneq ($(shell command -v $(CROSS)-gcc 2>/dev/null),)
CC := $(CROSS)-gcc
LD := $(CROSS)-ld
OBJCOPY := $(CROSS)-objcopy
else
CC := clang
LD := ld.lld
OBJCOPY := llvm-objcopy
CROSS_CFLAGS := --target=i686-none-elf
endif
CFLAGS := -m32 -std=c11 -ffreestanding -fno-builtin -fno-pic -fno-pie -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -mno-mmx -mno-sse -mno-sse2 -nostdlib -Wall -Wextra -Werror
CFLAGS += $(CROSS_CFLAGS)

BUILD_DIR := build
STAGE2_SECTORS ?= 12
STAGE2_BYTES := $(shell expr $(STAGE2_SECTORS) \* 512)
KERNEL_MAX_BYTES := 524288
FLOPPY_BYTES := 1474560
BOOT_BIN := $(BUILD_DIR)/boot.bin
STAGE2_BIN := $(BUILD_DIR)/stage2.bin
ENTRY_OBJ := $(BUILD_DIR)/kernel_entry.o
KERNEL_MAIN_OBJ := $(BUILD_DIR)/kernel.o
PANIC_OBJ := $(BUILD_DIR)/panic.o
SYSCALL_OBJ := $(BUILD_DIR)/syscall.o
IPC_OBJ := $(BUILD_DIR)/ipc.o
LANGUAGE_OBJ := $(BUILD_DIR)/language.o
SPLASH_OBJ := $(BUILD_DIR)/splash.o
VGA_OBJ := $(BUILD_DIR)/vga.o
SERIAL_OBJ := $(BUILD_DIR)/serial.o
KEYBOARD_OBJ := $(BUILD_DIR)/keyboard.o
PCI_OBJ := $(BUILD_DIR)/pci.o
BLOCK_DEVICE_OBJ := $(BUILD_DIR)/block_device.o
ATA_OBJ := $(BUILD_DIR)/ata.o
ATA_IRQ_STATE_OBJ := $(BUILD_DIR)/ata_irq_state.o
ATA_DMA_OBJ := $(BUILD_DIR)/ata_dma.o
RTC_OBJ := $(BUILD_DIR)/rtc.o
RTC_TIME_OBJ := $(BUILD_DIR)/rtc_time.o
ACPI_TABLES_OBJ := $(BUILD_DIR)/acpi_tables.o
ACPI_OBJ := $(BUILD_DIR)/acpi.o
IDT_OBJ := $(BUILD_DIR)/idt.o
PIT_OBJ := $(BUILD_DIR)/pit.o
SYSTEM_OBJ := $(BUILD_DIR)/system.o
FEATURES_OBJ := $(BUILD_DIR)/features.o
INTERRUPTS_OBJ := $(BUILD_DIR)/interrupts.o
PMM_OBJ := $(BUILD_DIR)/pmm.o
E820_OBJ := $(BUILD_DIR)/e820.o
PAGING_OBJ := $(BUILD_DIR)/paging.o
HEAP_OBJ := $(BUILD_DIR)/heap.o
SCHEDULER_OBJ := $(BUILD_DIR)/scheduler.o
USER_PROCESS_OBJ := $(BUILD_DIR)/user_process.o
USER_DEMO_OBJ := $(BUILD_DIR)/user_demo.o
ELF_LOADER_OBJ := $(BUILD_DIR)/elf_loader.o
PROGRAMS_OBJ := $(BUILD_DIR)/programs.o
RAMFS_OBJ := $(BUILD_DIR)/ramfs.o
JOURNAL_OBJ := $(BUILD_DIR)/journal.o
PERSISTENT_OBJ := $(BUILD_DIR)/persistent.o
FAT_OBJ := $(BUILD_DIR)/fat.o
VFS_OBJ := $(BUILD_DIR)/vfs.o
GAMES_OBJ := $(BUILD_DIR)/games.o
SHELL_OBJ := $(BUILD_DIR)/shell.o
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
IMAGE := $(BUILD_DIR)/efesos.img

.PHONY: all run clean verify sha256-self-test sha256-boot-negative-test

all: $(IMAGE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(ENTRY_OBJ): kernel/kernel_entry.asm | $(BUILD_DIR)
	$(NASM) -w+error -f elf32 $< -o $@

$(KERNEL_MAIN_OBJ): kernel/kernel.c include/acpi.h include/ata.h include/block_device.h include/boot_info.h cpu/features.h cpu/idt.h cpu/tss.h games/games.h include/keyboard.h include/pci.h include/rtc.h kernel/panic.h kernel/splash.h kernel/ipc.h memory/heap.h memory/paging.h memory/pmm.h process/elf_loader.h process/scheduler.h process/user_process.h fs/ramfs.h fs/journal.h fs/persistent.h fs/vfs.h include/serial.h include/syscall.h shell/shell.h include/vga.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -Ifs -Igames -Ikernel -Imemory -Iprocess -Ishell -c $< -o $@

$(PANIC_OBJ): kernel/panic.c kernel/panic.h include/serial.h include/vga.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Ikernel -c $< -o $@

$(SYSCALL_OBJ): kernel/syscall.c include/syscall.h cpu/idt.h cpu/pit.h process/scheduler.h memory/paging.h kernel/ipc.h include/serial.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -Ikernel -Imemory -Iprocess -c $< -o $@

$(IPC_OBJ): kernel/ipc.c kernel/ipc.h process/scheduler.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Ikernel -Iprocess -c $< -o $@

$(LANGUAGE_OBJ): kernel/language.c include/language.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

$(SPLASH_OBJ): kernel/splash.c kernel/splash.h shell/shell.h include/vga.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Ikernel -Ishell -c $< -o $@

$(VGA_OBJ): drivers/vga.c include/boot_info.h include/vga.h include/pci.h cpu/io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

$(SERIAL_OBJ): drivers/serial.c include/serial.h cpu/io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -c $< -o $@

$(KEYBOARD_OBJ): drivers/keyboard.c include/keyboard.h kernel/splash.h shell/shell.h include/vga.h cpu/io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -Ikernel -Ishell -c $< -o $@

$(PCI_OBJ): drivers/pci.c include/pci.h cpu/io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -c $< -o $@

$(BLOCK_DEVICE_OBJ): drivers/block_device.c include/block_device.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

$(ATA_OBJ): drivers/ata.c drivers/ata_irq_state.h drivers/ata_dma.h include/ata.h include/block_device.h include/pci.h cpu/io.h memory/pmm.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -Idrivers -Imemory -c $< -o $@

$(ATA_IRQ_STATE_OBJ): drivers/ata_irq_state.c drivers/ata_irq_state.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Idrivers -c $< -o $@

$(ATA_DMA_OBJ): drivers/ata_dma.c drivers/ata_dma.h include/pci.h cpu/io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -Idrivers -c $< -o $@

$(RTC_OBJ): drivers/rtc.c drivers/rtc_time.h include/rtc.h cpu/io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -Idrivers -c $< -o $@

$(RTC_TIME_OBJ): drivers/rtc_time.c drivers/rtc_time.h include/rtc.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Idrivers -c $< -o $@

$(ACPI_TABLES_OBJ): drivers/acpi_tables.c drivers/acpi_tables.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Idrivers -c $< -o $@

$(ACPI_OBJ): drivers/acpi.c drivers/acpi_tables.h include/acpi.h include/boot_info.h memory/paging.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Idrivers -Imemory -c $< -o $@

$(IDT_OBJ): cpu/idt.c cpu/idt.h cpu/io.h cpu/pit.h include/ata.h include/keyboard.h include/serial.h include/vga.h kernel/panic.h process/scheduler.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -Ikernel -Iprocess -c $< -o $@

$(PIT_OBJ): cpu/pit.c cpu/pit.h cpu/io.h process/scheduler.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Icpu -Iprocess -c $< -o $@

$(SYSTEM_OBJ): cpu/system.c cpu/system.h cpu/io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Icpu -c $< -o $@

$(FEATURES_OBJ): cpu/features.c cpu/features.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Icpu -c $< -o $@

$(INTERRUPTS_OBJ): cpu/interrupts.asm | $(BUILD_DIR)
	$(NASM) -w+error -f elf32 $< -o $@

$(PMM_OBJ): memory/pmm.c memory/pmm.h memory/e820.h include/boot_info.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Imemory -c $< -o $@

$(E820_OBJ): memory/e820.c memory/e820.h include/boot_info.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Imemory -c $< -o $@

$(PAGING_OBJ): memory/paging.c memory/paging.h memory/pmm.h include/boot_info.h cpu/features.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -Imemory -c $< -o $@

$(HEAP_OBJ): memory/heap.c memory/heap.h memory/paging.h memory/pmm.h include/boot_info.h kernel/panic.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Ikernel -Imemory -c $< -o $@

$(SCHEDULER_OBJ): process/scheduler.c process/scheduler.h process/user_process.h cpu/idt.h memory/heap.h memory/paging.h memory/pmm.h kernel/ipc.h kernel/panic.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -Ikernel -Imemory -Iprocess -c $< -o $@

$(USER_PROCESS_OBJ): process/user_process.c process/user_process.h process/scheduler.h process/elf_loader.h memory/paging.h memory/pmm.h kernel/panic.h cpu/pit.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -Ikernel -Imemory -Iprocess -c $< -o $@

$(ELF_LOADER_OBJ): process/elf_loader.c process/elf_loader.h memory/paging.h memory/pmm.h kernel/panic.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Ikernel -Iprocess -Imemory -c $< -o $@

$(USER_DEMO_OBJ): process/user_demo.asm | $(BUILD_DIR)
	$(NASM) -w+error -f elf32 $< -o $@

$(PROGRAMS_OBJ): process/programs.c process/programs.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iprocess -c $< -o $@

$(RAMFS_OBJ): fs/ramfs.c fs/ramfs.h fs/journal.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Ifs -c $< -o $@

$(JOURNAL_OBJ): fs/journal.c fs/journal.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Ifs -c $< -o $@

$(PERSISTENT_OBJ): fs/persistent.c fs/persistent.h fs/journal.h fs/ramfs.h fs/vfs.h include/ata.h include/serial.h kernel/panic.h memory/heap.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Ifs -Ikernel -Icpu -Imemory -c $< -o $@

$(FAT_OBJ): fs/fat.c fs/fat.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Ifs -c $< -o $@

$(VFS_OBJ): fs/vfs.c fs/vfs.h fs/fat.h include/block_device.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Ifs -Icpu -c $< -o $@

$(GAMES_OBJ): games/games.c games/games.h cpu/pit.h include/vga.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -Igames -c $< -o $@

$(SHELL_OBJ): shell/shell.c shell/shell.h include/ata.h include/keyboard.h include/language.h include/pci.h include/rtc.h include/serial.h include/vga.h cpu/pit.h cpu/system.h fs/ramfs.h fs/persistent.h fs/vfs.h games/games.h memory/heap.h memory/pmm.h process/programs.h process/scheduler.h process/user_process.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -Icpu -Ifs -Igames -Imemory -Iprocess -Ishell -c $< -o $@

$(KERNEL_ELF): $(ENTRY_OBJ) $(KERNEL_MAIN_OBJ) $(PANIC_OBJ) $(SYSCALL_OBJ) $(IPC_OBJ) $(LANGUAGE_OBJ) $(SPLASH_OBJ) $(VGA_OBJ) $(SERIAL_OBJ) $(KEYBOARD_OBJ) $(PCI_OBJ) $(BLOCK_DEVICE_OBJ) $(ATA_OBJ) $(ATA_IRQ_STATE_OBJ) $(ATA_DMA_OBJ) $(RTC_OBJ) $(RTC_TIME_OBJ) $(ACPI_TABLES_OBJ) $(ACPI_OBJ) $(IDT_OBJ) $(PIT_OBJ) $(SYSTEM_OBJ) $(FEATURES_OBJ) $(INTERRUPTS_OBJ) $(PMM_OBJ) $(E820_OBJ) $(PAGING_OBJ) $(HEAP_OBJ) $(SCHEDULER_OBJ) $(USER_PROCESS_OBJ) $(ELF_LOADER_OBJ) $(USER_DEMO_OBJ) $(PROGRAMS_OBJ) $(RAMFS_OBJ) $(JOURNAL_OBJ) $(PERSISTENT_OBJ) $(FAT_OBJ) $(VFS_OBJ) $(GAMES_OBJ) $(SHELL_OBJ) kernel/linker.ld
	$(LD) -m elf_i386 -T kernel/linker.ld -o $@ $(ENTRY_OBJ) $(KERNEL_MAIN_OBJ) $(PANIC_OBJ) $(SYSCALL_OBJ) $(IPC_OBJ) $(LANGUAGE_OBJ) $(SPLASH_OBJ) $(VGA_OBJ) $(SERIAL_OBJ) $(KEYBOARD_OBJ) $(PCI_OBJ) $(BLOCK_DEVICE_OBJ) $(ATA_OBJ) $(ATA_IRQ_STATE_OBJ) $(ATA_DMA_OBJ) $(RTC_OBJ) $(RTC_TIME_OBJ) $(ACPI_TABLES_OBJ) $(ACPI_OBJ) $(IDT_OBJ) $(PIT_OBJ) $(SYSTEM_OBJ) $(FEATURES_OBJ) $(INTERRUPTS_OBJ) $(PMM_OBJ) $(E820_OBJ) $(PAGING_OBJ) $(HEAP_OBJ) $(SCHEDULER_OBJ) $(USER_PROCESS_OBJ) $(ELF_LOADER_OBJ) $(USER_DEMO_OBJ) $(PROGRAMS_OBJ) $(RAMFS_OBJ) $(JOURNAL_OBJ) $(PERSISTENT_OBJ) $(FAT_OBJ) $(VFS_OBJ) $(GAMES_OBJ) $(SHELL_OBJ)

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@
	test $$(wc -c < $@) -le $(KERNEL_MAX_BYTES)
	sectors=$$((($$(wc -c < $@) + 511) / 512)); truncate -s $$((sectors * 512)) $@

$(BOOT_BIN): boot/boot.asm | $(BUILD_DIR)
	$(NASM) -w+error -D STAGE2_SECTORS=$(STAGE2_SECTORS) -f bin $< -o $@

$(STAGE2_BIN): boot/stage2.asm $(KERNEL_BIN) scripts/sha256_words.py | $(BUILD_DIR)
	sectors=$$(($$(wc -c < $(KERNEL_BIN)) / 512)); set -- $$($(PYTHON) scripts/sha256_words.py $(KERNEL_BIN)); $(NASM) -w+error -D STAGE2_SECTORS=$(STAGE2_SECTORS) -D KERNEL_SECTORS=$$sectors -D KERNEL_SHA256_0=$$1 -D KERNEL_SHA256_1=$$2 -D KERNEL_SHA256_2=$$3 -D KERNEL_SHA256_3=$$4 -D KERNEL_SHA256_4=$$5 -D KERNEL_SHA256_5=$$6 -D KERNEL_SHA256_6=$$7 -D KERNEL_SHA256_7=$$8 -f bin $< -o $@

$(IMAGE): $(BOOT_BIN) $(STAGE2_BIN) $(KERNEL_BIN)
	truncate -s $(FLOPPY_BYTES) $(IMAGE)
	dd if=/dev/zero of=$(IMAGE) bs=512 count=$$(( $(FLOPPY_BYTES) / 512 )) conv=notrunc status=none
	dd if=$(BOOT_BIN) of=$(IMAGE) conv=notrunc status=none
	dd if=$(STAGE2_BIN) of=$(IMAGE) bs=512 seek=1 conv=notrunc status=none
	dd if=$(KERNEL_BIN) of=$(IMAGE) bs=512 seek=$$((1 + $(STAGE2_SECTORS))) conv=notrunc status=none
	$(MAKE) verify

verify: $(BOOT_BIN) $(STAGE2_BIN) $(KERNEL_BIN) $(IMAGE)
	test $$(wc -c < $(BOOT_BIN)) -eq 512
	test $$(wc -c < $(STAGE2_BIN)) -eq $(STAGE2_BYTES)
	test $$(($$(wc -c < $(KERNEL_BIN)) % 512)) -eq 0
	test $$(wc -c < $(IMAGE)) -eq $(FLOPPY_BYTES)
	test "$$(od -An -tx1 -j510 -N2 $(BOOT_BIN) | tr -d ' \\n')" = "55aa"

sha256-self-test: $(KERNEL_BIN)
	$(PYTHON) scripts/sha256_self_test.py $(KERNEL_BIN)

sha256-boot-negative-test: $(IMAGE)
	$(PYTHON) scripts/sha256_boot_negative_test.py

run: $(IMAGE)
	$(QEMU) -vga std -drive file=$(IMAGE),format=raw,if=floppy -boot a -no-reboot -no-shutdown

clean:
	rm -rf $(BUILD_DIR)
