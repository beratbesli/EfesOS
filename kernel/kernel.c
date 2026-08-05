#include "idt.h"
#include "keyboard.h"
#include "paging.h"
#include "pmm.h"
#include "scheduler.h"
#include "shell.h"
#include "vga.h"

static const char boot_banner[] =
    "BBBBBB   EEEEEEE  EEEEEEE  RRRRRR    OOOOO   SSSSSS\n"
    "BB   BB  EE       EE       RR   RR  OO   OO SS\n"
    "BBBBBB   EEEEE    EEEEE    RRRRRR   OO   OO  SSSSS\n"
    "BB   BB  EE       EE       RR  RR   OO   OO      SS\n"
    "BBBBBB   EEEEEEE  EEEEEEE RR   RR   OOOOO  SSSSSS\n\n";

static void scheduler_task_one(void)
{
    vga_write("BeerOS: scheduler gorev 1 calisti.\n");
}

static void scheduler_task_two(void)
{
    vga_write("BeerOS: scheduler gorev 2 calisti.\n");
}

void kernel_main(void)
{
    vga_clear();
    vga_write(boot_banner);
    vga_write("BeerOS: protected mode ve VGA driver hazir.\n");

    pmm_init();
    if (!pmm_self_test()) {
        vga_write("BeerOS: fiziksel bellek yoneticisi hatasi.\n");
        return;
    }
    vga_write("BeerOS: fiziksel bellek yoneticisi calisiyor.\n");

    if (!paging_init()) {
        vga_write("BeerOS: paging hatasi.\n");
        return;
    }
    vga_write("BeerOS: paging aktif.\n");

    idt_init();
    __asm__ volatile ("int $0x30");

    scheduler_init();
    scheduler_add_task(scheduler_task_one);
    scheduler_add_task(scheduler_task_two);
    scheduler_run();

    keyboard_init();
    shell_init();
    __asm__ volatile ("sti");
}
