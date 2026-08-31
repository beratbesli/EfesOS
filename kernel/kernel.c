#include "boot_info.h"
#include "ata.h"
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
#include "tss.h"
#include "user_process.h"
#include "elf_loader.h"
#include "vfs.h"
#include "fat.h"
#include "shell.h"
#include "splash.h"
#include "vga.h"

static int scheduler_runtime_verified;
static int user_runtime_verified;
static int user_pointer_runtime_verified;
static int user_reap_runtime_verified;
static int user_address_space_runtime_verified;
static pit_tick_t last_game_tick;

static void verify_mounted_disk_read(void)
{
    char name[13];
    char contents[65];
    unsigned int size;

    if (!vfs_is_mounted()) {
        return;
    }
    if (vfs_file_count() == 0U ||
        !vfs_file_name(0, name, sizeof(name)) ||
        !vfs_read_file(name, contents, sizeof(contents) - 1U, &size)) {
        return;
    }
    contents[size] = '\0';
    serial_write("EfesOS: FAT directory/file read self-test passed (file=");
    serial_write(name);
    serial_write(").\n");
}

static void kernel_wait_for_work(void);
static void kernel_process_events(void);

static void kernel_event_task(void)
{
    for (;;) {
        kernel_wait_for_work();
        kernel_process_events();
    }
}

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
    if (!user_runtime_verified && syscall_user_call_count() != 0U) {
        user_runtime_verified = 1;
        serial_write("EfesOS: ring3 syscall runtime test passed.\n");
    }
    if (!user_pointer_runtime_verified && syscall_user_pointer_reject_count() != 0U) {
        user_pointer_runtime_verified = 1;
        serial_write("EfesOS: user pointer validation runtime test passed.\n");
    }
    if (!user_reap_runtime_verified && user_process_reap_count() != 0U) {
        user_reap_runtime_verified = 1;
        serial_write("EfesOS: user process resource cleanup passed.\n");
    }
    if (!user_address_space_runtime_verified && syscall_user_address_space_call_count() != 0U) {
        user_address_space_runtime_verified = 1;
        serial_write("EfesOS: user address-space switch runtime test passed.\n");
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
    tss_init();

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
    if (!elf_loader_self_test()) {
        kernel_panic("ELF loader self-test failed.");
    }
    serial_write("EfesOS: ELF loader validation self-test passed.\n");

    pci_init();
    serial_write("EfesOS: PCI devices discovered=");
    serial_write_hex(pci_device_count());
    serial_write("\n");
    ata_init();
    serial_write("EfesOS: ATA primary-master present=");
    serial_write_hex(ata_present());
    serial_write(" sectors=");
    serial_write_hex(ata_sector_count());
    serial_write(" status=");
    serial_write_hex(ata_last_status());
    serial_write(" type=");
    serial_write_hex(ata_identify_type());
    serial_write("\n");
    vfs_init();
    serial_write("EfesOS: FAT volume mounted=");
    serial_write_hex(vfs_is_mounted());
    serial_write(" error=");
    serial_write_hex(fat_last_error());
    serial_write("\n");
    verify_mounted_disk_read();

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
    if (!elf_loader_runtime_self_test()) {
        kernel_panic("ELF loader runtime self-test failed.");
    }
    serial_write("EfesOS: ELF loader runtime self-test passed.\n");
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
    user_runtime_verified = 0;
    user_pointer_runtime_verified = 0;
    user_reap_runtime_verified = 0;
    user_address_space_runtime_verified = 0;
    programs_init();
    ramfs_init();
    if (!ramfs_self_test()) {
        kernel_panic("RAM filesystem self-test failed.");
    }
    serial_write("EfesOS: RAM filesystem self-test passed.\n");
    scheduler_add_task("counter", counter_program);
    scheduler_add_task("snake", snake_program);
    if (!user_process_init()) {
        kernel_panic("User process initialization failed.");
    }
    if (user_process_address_space() == 0U ||
        user_process_address_space() == paging_kernel_directory()) {
        kernel_panic("User process address-space isolation failed.");
    }
    serial_write("EfesOS: user address-space isolation self-test passed.\n");
    scheduler_add_task("event-loop", kernel_event_task);
    if (!scheduler_set_priority(1U, 2U) || !scheduler_set_priority(2U, 1U)) {
        kernel_panic("Scheduler priority setup failed.");
    }
    serial_write("EfesOS: scheduler priority self-test passed.\n");
    last_game_tick = 0;

    keyboard_init();
    vga_write("EfesOS: scheduler and game loop running.\n");
    shell_init();
    pit_init();
    serial_write("EfesOS: deferred event loop ready.\n");
    scheduler_start();

    for (;;) {
        __asm__ volatile ("sti\n\thlt" : : : "memory");
    }
}
