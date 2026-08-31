#include "boot_info.h"
#include "idt.h"
#include "games.h"
#include "heap.h"
#include "keyboard.h"
#include "paging.h"
#include "panic.h"
#include "pit.h"
#include "pmm.h"
#include "pci.h"
#include "programs.h"
#include "ramfs.h"
#include "scheduler.h"
#include "serial.h"
#include "syscall.h"
#include "shell.h"
#include "splash.h"
#include "vga.h"

static int scheduler_runtime_verified;
static pit_tick_t last_game_tick;

static int kernel_work_pending(void)
{
    return keyboard_has_pending() || pit_ticks() != last_game_tick;
}

static void kernel_wait_for_work(void)
{
    __asm__ volatile ("cli" : : : "memory");
    if (!kernel_work_pending()) {
        __asm__ volatile ("sti\n\thlt" : : : "memory");
    } else {
        __asm__ volatile ("sti" : : : "memory");
    }
}

static void kernel_process_events(void)
{
    unsigned int processed_input = 0;
    unsigned char character;

    while (processed_input < 32U && keyboard_read_char(&character)) {
        shell_handle_char(character);
        processed_input++;
    }
    if (!scheduler_runtime_verified && counter_program_runs() != 0U &&
        snake_program_steps() != 0U && scheduler_task_runs(1U) != 0U &&
        scheduler_task_runs(2U) != 0U) {
        scheduler_runtime_verified = 1;
        serial_write("EfesOS: preemptive scheduler runtime test passed.\n");
    }
    if (pit_ticks() != last_game_tick) {
        last_game_tick = pit_ticks();
        games_tick();
    }
}

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

    pci_init();
    serial_write("EfesOS: PCI devices discovered=");
    serial_write_hex(pci_device_count());
    serial_write("\n");

    idt_init();
    syscall_init();
    __asm__ volatile ("int $0x03");
    __asm__ volatile ("int $0x30");
    {
        unsigned int syscall_ticks;
        unsigned int invalid_syscall;

        __asm__ volatile ("xor %%eax, %%eax\n\tint $0x80" : "=a"(syscall_ticks) : : "memory");
        __asm__ volatile ("mov $0xFFFFFFFF, %%eax\n\tint $0x80" : "=a"(invalid_syscall) : : "memory");
        if (syscall_ticks != pit_ticks() || invalid_syscall != 0xFFFFFFFFU) {
            kernel_panic("Syscall ABI self-test failed.");
        }
        serial_write("EfesOS: syscall ABI self-test passed.\n");
    }
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
    scheduler_runtime_verified = 0;
    programs_init();
    ramfs_init();
    if (!ramfs_self_test()) {
        kernel_panic("RAM filesystem self-test failed.");
    }
    serial_write("EfesOS: RAM filesystem self-test passed.\n");
    scheduler_add_task("counter", counter_program);
    scheduler_add_task("snake", snake_program);
    last_game_tick = 0;
    scheduler_start();

    keyboard_init();
    vga_write("EfesOS: scheduler and game loop running.\n");
    shell_init();
    pit_init();
    serial_write("EfesOS: deferred event loop ready.\n");

    for (;;) {
        kernel_wait_for_work();
        kernel_process_events();
    }
}
