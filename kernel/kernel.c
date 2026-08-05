#include "idt.h"
#include "keyboard.h"
#include "paging.h"
#include "pmm.h"
#include "scheduler.h"
#include "shell.h"
#include "vga.h"

static const char boot_banner[] =
    "BBBBBBBBBBBBBBBBB                                                                    OOOOOOOOO        SSSSSSSSSSSSSSS \n"
    "B::::::::::::::::B                                                                 OO:::::::::OO    SS:::::::::::::::S\n"
    "B::::::BBBBBB:::::B                                                              OO:::::::::::::OO S:::::SSSSSS::::::S\n"
    "BB:::::B     B:::::B                                                            O:::::::OOO:::::::OS:::::S     SSSSSSS\n"
    "  B::::B     B:::::B    eeeeeeeeeeee        eeeeeeeeeeee    rrrrr   rrrrrrrrr   O::::::O   O::::::OS:::::S            \n"
    "  B::::B     B:::::B  ee::::::::::::ee    ee::::::::::::ee  r::::rrr:::::::::r  O:::::O     O:::::OS:::::S            \n"
    "  B::::BBBBBB:::::B  e::::::eeeee:::::ee e::::::eeeee:::::eer:::::::::::::::::r O:::::O     O:::::O S::::SSSS         \n"
    "  B:::::::::::::BB  e::::::e     e:::::ee::::::e     e:::::err::::::rrrrr::::::rO:::::O     O:::::O  SS::::::SSSSS    \n"
    "  B::::BBBBBB:::::B e:::::::eeeee::::::ee:::::::eeeee::::::e r:::::r     r:::::rO:::::O     O:::::O    SSS::::::::SS  \n"
    "  B::::B     B:::::Be:::::::::::::::::e e:::::::::::::::::e  r:::::r     rrrrrrrO:::::O     O:::::O       SSSSSS::::S \n"
    "  B::::B     B:::::Be::::::eeeeeeeeeee  e::::::eeeeeeeeeee   r:::::r            O:::::O     O:::::O            S:::::S\n"
    "  B::::B     B:::::Be:::::::e           e:::::::e            r:::::r            O::::::O   O::::::O            S:::::S\n"
    "BB:::::BBBBBB::::::Be::::::::e          e::::::::e           r:::::r            O:::::::OOO:::::::OSSSSSSS     S:::::S\n"
    "B:::::::::::::::::B  e::::::::eeeeeeee   e::::::::eeeeeeee   r:::::r             OO:::::::::::::OO S::::::SSSSSS:::::S\n"
    "B::::::::::::::::B    ee:::::::::::::e    ee:::::::::::::e   r:::::r               OO:::::::::OO   S:::::::::::::::SS \n"
    "BBBBBBBBBBBBBBBBB       eeeeeeeeeeeeee      eeeeeeeeeeeeee   rrrrrrr                 OOOOOOOOO      SSSSSSSSSSSSSSS  \n";

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
    pmm_init();
    if (!pmm_self_test()) {
        return;
    }

    if (!paging_init()) {
        return;
    }

    vga_init();
    vga_clear();
    vga_write(boot_banner);
    vga_write("BeerOS: protected mode ve VGA driver hazir.\n");
    vga_write("BeerOS: fiziksel bellek yoneticisi calisiyor.\n");
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
