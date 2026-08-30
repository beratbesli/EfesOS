#include "idt.h"
#include "games.h"
#include "keyboard.h"
#include "paging.h"
#include "pit.h"
#include "pmm.h"
#include "programs.h"
#include "scheduler.h"
#include "shell.h"
#include "splash.h"
#include "vga.h"

void kernel_main(void)
{
    pmm_init();
    if (!pmm_self_test()) {
        return;
    }

    if (!paging_init()) {
        return;
    }

    vga_init();
    vga_clear();
    splash_show();
    vga_write("EfesOS: protected mode and VGA driver ready.\n");
    vga_write("EfesOS: physical memory manager running.\n");
    vga_write("EfesOS: paging enabled.\n");

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
