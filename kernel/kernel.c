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
#include "ipc.h"
#include "vfs.h"
#include "fat.h"
#include "shell.h"
#include "splash.h"
#include "vga.h"

#define USER_PROCESS_ALLOCATION_BLOCKS 9U
#define USER_PROCESS_RESTART_TARGET 4U

static int scheduler_runtime_verified;
static int user_runtime_verified;
static int user_pointer_runtime_verified;
static int user_reap_runtime_verified;
static unsigned int user_restart_count;
static int user_repeated_reap_runtime_verified;
static int user_restart_stress_runtime_verified;
static unsigned int user_process_initial_free_blocks;
static unsigned int user_restart_wait_ticks;
static int user_address_space_runtime_verified;
static int user_ipc_runtime_verified;
static int user_ipc_reject_runtime_verified;
static int user_ipc_target_runtime_verified;
static int user_ipc_wait_runtime_verified;
static int user_pid_runtime_verified;
static int scheduler_stack_reap_runtime_verified;
static int scheduler_block_runtime_verified;
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
    if (user_restart_count < USER_PROCESS_RESTART_TARGET &&
        user_process_reap_count() > user_restart_count) {
        unsigned int pending_restarts = user_process_reap_count() - user_restart_count;
        unsigned int expected_free = user_process_initial_free_blocks +
            USER_PROCESS_ALLOCATION_BLOCKS * pending_restarts;

        if (pmm_free_blocks() < user_process_initial_free_blocks ||
            user_restart_wait_ticks++ > 200U) {
            serial_write("EfesOS: restart accounting baseline=");
            serial_write_hex(user_process_initial_free_blocks);
            serial_write(" expected=");
            serial_write_hex(expected_free);
            serial_write(" current=");
            serial_write_hex(pmm_free_blocks());
            serial_write(" waits=");
            serial_write_hex(user_restart_wait_ticks);
            serial_write("\n");
            kernel_panic("User process restart leaked physical memory.");
        }
        if (pmm_free_blocks() == expected_free) {
            if (!user_process_init()) {
                kernel_panic("User process restart failed.");
            }
            if (pmm_free_blocks() != expected_free - USER_PROCESS_ALLOCATION_BLOCKS) {
                kernel_panic("User process restart leaked physical memory.");
            }
            user_restart_count++;
            user_restart_wait_ticks = 0U;
            serial_write("EfesOS: user process restart and slot reuse passed.\n");
        }
    }
    if (!user_repeated_reap_runtime_verified && user_process_reap_count() >= 2U) {
        user_repeated_reap_runtime_verified = 1;
        serial_write("EfesOS: repeated user process cleanup passed.\n");
    }
    if (!user_restart_stress_runtime_verified &&
        user_restart_count >= USER_PROCESS_RESTART_TARGET &&
        user_process_reap_count() >= USER_PROCESS_RESTART_TARGET) {
        user_restart_stress_runtime_verified = 1;
        serial_write("EfesOS: repeated user restart stress passed.\n");
    }
    if (!user_address_space_runtime_verified && syscall_user_address_space_call_count() != 0U) {
        user_address_space_runtime_verified = 1;
        serial_write("EfesOS: user address-space switch runtime test passed.\n");
    }
    if (!user_ipc_runtime_verified && syscall_user_ipc_call_count() >= 2U) {
        user_ipc_runtime_verified = 1;
        serial_write("EfesOS: user IPC syscall runtime test passed.\n");
    }
    if (!user_ipc_reject_runtime_verified && syscall_user_ipc_reject_count() != 0U) {
        user_ipc_reject_runtime_verified = 1;
        serial_write("EfesOS: invalid user IPC pointer rejected.\n");
    }
    if (!user_ipc_target_runtime_verified && syscall_user_ipc_target_count() != 0U) {
        user_ipc_target_runtime_verified = 1;
        serial_write("EfesOS: targeted user IPC runtime test passed.\n");
    }
    if (!user_ipc_wait_runtime_verified && syscall_user_ipc_block_count() != 0U) {
        user_ipc_wait_runtime_verified = 1;
        serial_write("EfesOS: blocking user IPC runtime test passed.\n");
    }
    if (!user_pid_runtime_verified && syscall_user_pid_call_count() >= 2U) {
        user_pid_runtime_verified = 1;
        serial_write("EfesOS: generation-based user PID runtime test passed.\n");
    }
    if (!scheduler_stack_reap_runtime_verified && scheduler_stack_reap_count() != 0U) {
        scheduler_stack_reap_runtime_verified = 1;
        serial_write("EfesOS: scheduler stack resource cleanup passed.\n");
    }
    if (!scheduler_block_runtime_verified && scheduler_blocked_count() == 0U) {
        scheduler_block_runtime_verified = 1;
        serial_write("EfesOS: scheduler block/wake lifecycle self-test passed.\n");
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
    user_restart_count = 0U;
    user_restart_wait_ticks = 0U;
    user_repeated_reap_runtime_verified = 0;
    user_restart_stress_runtime_verified = 0;
    user_address_space_runtime_verified = 0;
    user_ipc_runtime_verified = 0;
    user_ipc_reject_runtime_verified = 0;
    user_ipc_target_runtime_verified = 0;
    user_ipc_wait_runtime_verified = 0;
    user_pid_runtime_verified = 0;
    scheduler_stack_reap_runtime_verified = 0;
    scheduler_block_runtime_verified = 0;
    programs_init();
    ramfs_init();
    if (!ramfs_self_test()) {
        kernel_panic("RAM filesystem self-test failed.");
    }
    serial_write("EfesOS: RAM filesystem self-test passed.\n");
    ipc_init();
    if (!ipc_self_test()) {
        kernel_panic("IPC queue self-test failed.");
    }
    serial_write("EfesOS: bounded IPC queue self-test passed.\n");
    scheduler_add_task("counter", counter_program);
    scheduler_add_task("snake", snake_program);
    if (!user_process_init() || !user_process_init()) {
        kernel_panic("User process initialization failed.");
    }
    if (user_process_address_space() == 0U ||
        user_process_address_space() == paging_kernel_directory()) {
        kernel_panic("User process address-space isolation failed.");
    }
    if (user_process_active_count() != 2U ||
        user_process_address_space_at(0U) == 0U ||
        user_process_address_space_at(0U) == user_process_address_space_at(1U) ||
        user_process_stack_address_at(0U) == 0U ||
        user_process_stack_address_at(0U) == user_process_stack_address_at(1U)) {
        kernel_panic("Multiple user process initialization failed.");
    }
    serial_write("EfesOS: multiple user process isolation self-test passed.\n");
    if (scheduler_add_user_task_in_space("invalid-kernel-space", 0x00400000U,
        0x00801000U, paging_kernel_directory())) {
        kernel_panic("Kernel address space accepted for user task.");
    }
    if (scheduler_add_user_task_in_space("invalid-kernel-entry", 0xC0000000U,
        0x00801000U, user_process_address_space())) {
        kernel_panic("Kernel entry accepted for user task.");
    }
    serial_write("EfesOS: user address-space isolation self-test passed.\n");
    scheduler_add_task("event-loop", kernel_event_task);
    if (!scheduler_set_priority(1U, 2U) || !scheduler_set_priority(2U, 1U)) {
        kernel_panic("Scheduler priority setup failed.");
    }
    if (!scheduler_block_task(2U) || scheduler_blocked_count() != 1U ||
        !scheduler_wake_task(2U) || scheduler_blocked_count() != 0U) {
        kernel_panic("Scheduler block/wake self-test failed.");
    }
    user_process_initial_free_blocks = pmm_free_blocks();
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
