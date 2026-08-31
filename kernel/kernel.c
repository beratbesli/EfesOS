#include "boot_info.h"
#include "idt.h"
#include "games.h"
#include "heap.h"
#include "keyboard.h"
#include "paging.h"
#include "panic.h"
#include "pit.h"
#include "pmm.h"
#include "programs.h"
#include "scheduler.h"
#include "serial.h"
#include "shell.h"
#include "splash.h"
#include "vga.h"

void kernel_main(const struct boot_info *boot_info)
{
    serial_init();
    serial_write("EfesOS: kernel entry reached.\n");

    if (!boot_info_is_valid(boot_info)) {
        kernel_panic("Invalid or missing BIOS E820 boot information.");
    }

    serial_write("EfesOS: BIOS E820 entries available.\n");
    if (!pmm_init(boot_info)) {
        kernel_panic("BIOS memory map contains no usable physical memory.");
    }
    serial_write("EfesOS: PMM blocks total=");
    serial_write_hex(pmm_total_blocks());
    serial_write(" free=");
    serial_write_hex(pmm_free_blocks());
    serial_write("\n");
    if (!pmm_self_test()) {
        kernel_panic("Physical memory manager self-test failed.");
    }

    idt_init();
    __asm__ volatile ("int $0x03");
    __asm__ volatile ("int $0x30");
    serial_write("EfesOS: interrupt self-tests passed.\n");

    if (!paging_init(boot_info)) {
        kernel_panic("Paging initialization failed.");
    }
    if (!paging_self_test()) {
        kernel_panic("Virtual memory manager self-test failed.");
    }
    serial_write("EfesOS: VMM self-test passed.\n");
    if (!heap_init() || !heap_self_test()) {
        kernel_panic("Kernel heap self-test failed.");
    }
    serial_write("EfesOS: kernel heap self-test passed.\n");

    vga_init(boot_info);
    vga_clear();
    splash_show();
    vga_write("EfesOS: protected mode and VGA driver ready.\n");
    vga_write("EfesOS: physical memory manager running.\n");
    vga_write("EfesOS: paging enabled.\n");
    serial_write("EfesOS: paging enabled.\n");

    scheduler_init();
    programs_init();
    scheduler_add_task("counter", counter_program);
    scheduler_add_task("snake", snake_program);
    scheduler_add_task("games", games_tick);
    scheduler_start();

    keyboard_init();
    vga_write("EfesOS: scheduler and game loop running.\n");
    shell_init();
    pit_init();
    __asm__ volatile ("sti");
}
