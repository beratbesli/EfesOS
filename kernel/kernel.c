#include "boot_info.h"
#include "idt.h"
#include "games.h"
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
    pmm_init();
    if (!pmm_self_test()) {
        kernel_panic("Physical memory manager self-test failed.");
    }

    if (!paging_init()) {
        kernel_panic("Paging initialization failed.");
    }

    vga_init();
    vga_clear();
    splash_show();
    vga_write("EfesOS: protected mode and VGA driver ready.\n");
    vga_write("EfesOS: physical memory manager running.\n");
    vga_write("EfesOS: paging enabled.\n");
    serial_write("EfesOS: paging enabled.\n");

    idt_init();
    __asm__ volatile ("int $0x30");

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
