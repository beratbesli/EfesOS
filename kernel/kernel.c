#include "boot_info.h"
#include "features.h"
#include "acpi.h"
#include "ahci.h"
#include "ata.h"
#include "idt.h"
#include "games.h"
#include "hpet.h"
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
#include "rtc.h"
#include "syscall.h"
#include "tss.h"
#include "user_process.h"
#include "elf_loader.h"
#include "ipc.h"
#include "vfs.h"
#include "fat.h"
#include "journal.h"
#include "persistent.h"
#include "shell.h"
#include "splash.h"
#include "vga.h"

#define USER_PROCESS_RESTART_TARGET 4U

static int scheduler_runtime_verified;
static int user_runtime_verified;
static int user_pointer_runtime_verified;
static int user_reap_runtime_verified;
static unsigned int user_restart_count;
static int user_repeated_reap_runtime_verified;
static int user_restart_stress_runtime_verified;
static unsigned int user_process_initial_free_blocks;
static unsigned int user_process_allocation_blocks;
static unsigned int user_restart_wait_ticks;
static int user_address_space_runtime_verified;
static int user_ipc_runtime_verified;
static int user_ipc_reject_runtime_verified;
static int user_ipc_target_runtime_verified;
static int user_ipc_wait_runtime_verified;
static int user_exit_runtime_verified;
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
    serial_write("EfesOS: deferred event loop ready.\n");
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
            user_process_allocation_blocks * pending_restarts;

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
            unsigned int before_restart = pmm_free_blocks();

            if (!user_process_init()) {
                kernel_panic("User process restart failed.");
            }
            if (pmm_free_blocks() !=
                before_restart - user_process_allocation_blocks) {
                serial_write("EfesOS: restart allocation expected=");
                serial_write_hex(user_process_allocation_blocks);
                serial_write(" consumed=");
                serial_write_hex(before_restart - pmm_free_blocks());
                serial_write(".\n");
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
    if (!user_exit_runtime_verified && syscall_user_exit_count() != 0U) {
        user_exit_runtime_verified = 1;
        serial_write("EfesOS: user exit lifecycle runtime test passed.\n");
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
    cpu_features_init();
    {
        const struct cpu_features *features = cpu_features_get();

        serial_write("EfesOS: CPU features cpuid=");
        serial_write_hex(features->cpuid);
        serial_write(" pae=");
        serial_write_hex(features->pae);
        serial_write(" nx=");
        serial_write_hex(features->nx);
        serial_write(" tsc=");
        serial_write_hex(features->tsc);
        serial_write(" rdrand=");
        serial_write_hex(features->rdrand);
        serial_write(" msr=");
        serial_write_hex(features->msr);
        serial_write(" apic=");
        serial_write_hex(features->apic);
        serial_write(" x2apic=");
        serial_write_hex(features->x2apic);
        serial_write(" (reported; paging mode is selected after capability checks).\n");
    }
    tss_init();

    if (!boot_info_is_valid(boot_info)) {
        kernel_panic("Invalid or missing BIOS E820 boot information.");
    }
    if ((boot_info->video_flags & BOOT_KERNEL_INTEGRITY_VERIFIED) == 0U) {
        kernel_panic("Kernel integrity check was not verified by stage-2.");
    }

    serial_write("EfesOS: BIOS E820 entries available.\n");
    serial_write("EfesOS: stage-2 kernel integrity check passed.\n");
    if (!rtc_self_test()) {
        kernel_panic("RTC calendar self-test failed.");
    }
    serial_write("EfesOS: RTC calendar self-test passed.\n");
    if (rtc_init()) {
        struct rtc_time wall_clock;

        if (rtc_read_time(&wall_clock)) {
            serial_write("EfesOS: RTC stable read passed year=");
            serial_write_hex(wall_clock.year);
            serial_write(" month=");
            serial_write_hex(wall_clock.month);
            serial_write(" day=");
            serial_write_hex(wall_clock.day);
            serial_write(" hour=");
            serial_write_hex(wall_clock.hour);
            serial_write(" minute=");
            serial_write_hex(wall_clock.minute);
            serial_write(" second=");
            serial_write_hex(wall_clock.second);
            serial_write(".\n");
        } else {
            serial_write("EfesOS: RTC became unstable after initialization; wall clock disabled.\n");
        }
    } else {
        serial_write("EfesOS: RTC unavailable or invalid; wall clock disabled.\n");
    }
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
    if (!pci_self_test()) {
        kernel_panic("PCI BAR discovery self-test failed.");
    }
    serial_write("EfesOS: PCI devices discovered=");
    serial_write_hex(pci_device_count());
    serial_write("\n");
    serial_write("EfesOS: AHCI controllers discovered=");
    serial_write_hex(pci_ahci_controller_count());
    serial_write(" usable-mmio=");
    serial_write_hex(pci_ahci_usable_count());
    serial_write(".\n");
    serial_write("EfesOS: PCI BAR self-test passed.\n");
    ata_init();
    if (!ata_write_protected()) {
        kernel_panic("ATA write protection is not active.");
    }
    {
        unsigned char write_probe[512];

        if (ata_write_sectors(0U, 1U, write_probe)) {
            kernel_panic("ATA write-protected path accepted a write.");
        }
        serial_write("EfesOS: ATA write path fail-closed self-test passed.\n");
    }
    serial_write("EfesOS: ATA primary-master present=");
    serial_write_hex(ata_present());
    serial_write(" sectors=");
    serial_write_hex(ata_sector_count());
        serial_write(" status=");
        serial_write_hex(ata_last_status());
        serial_write(" type=");
        serial_write_hex(ata_identify_type());
        serial_write(" lba48=");
        serial_write_hex((unsigned int)ata_lba48_supported());
        serial_write(" write-protected=");
    serial_write_hex(ata_write_protected());
    serial_write("\n");
    {
        const struct block_device *boot_device = ata_block_device();

        if ((ata_present() && (!block_device_is_ready(boot_device) ||
                block_device_sector_count(boot_device) != ata_sector_count() ||
                block_device_can_write(boot_device))) ||
            (!ata_present() && boot_device != 0)) {
            kernel_panic("ATA block device contract failed.");
        }
        serial_write("EfesOS: block device abstraction passed.\n");
        vfs_init(boot_device);
    }
    serial_write("EfesOS: FAT volume mounted=");
    serial_write_hex(vfs_is_mounted());
    serial_write(" error=");
    serial_write_hex(fat_last_error());
    serial_write("\n");
    verify_mounted_disk_read();

    idt_init();
    if (idt_enable_irq_line(7U)) {
        kernel_panic("Unhandled PIC line was enabled.");
    }
    if (ata_present()) {
        unsigned int pio_probe_physical = pmm_alloc_block_below(0x00400000U);
        unsigned int irq_probe_physical = pmm_alloc_block_below(0x00400000U);
        unsigned char *pio_probe = (unsigned char *)pio_probe_physical;
        unsigned char *irq_probe = (unsigned char *)irq_probe_physical;
        unsigned char probe_sectors = ata_sector_count() >= 8U ? 8U : 1U;
        unsigned int probe_bytes = (unsigned int)probe_sectors * ATA_SECTOR_SIZE;
        unsigned int irq_before;
        unsigned int dma_before;
        unsigned int byte_index;
        int irq_read_ok;

        if (pio_probe_physical == 0U || irq_probe_physical == 0U) {
            kernel_panic("ATA integrity probe allocation failed.");
        }
        if (!ata_read_sectors(0U, probe_sectors, pio_probe)) {
            kernel_panic("ATA PIO reference read failed.");
        }
        if (!idt_enable_irq_line(14U) || !idt_irq_line_enabled(14U) ||
            !ata_enable_irq_mode()) {
            kernel_panic("ATA IRQ14 setup failed.");
        }
        __asm__ volatile ("sti" : : : "memory");
        (void)ata_enable_dma_mode();
        irq_before = ata_irq_count();
        dma_before = ata_dma_transfer_count();
        irq_read_ok = ata_read_sectors(0U, probe_sectors, irq_probe);
        __asm__ volatile ("cli" : : : "memory");
        if (!irq_read_ok || ata_irq_count() == irq_before) {
            kernel_panic("ATA IRQ completion self-test failed.");
        }
        for (byte_index = 0U; byte_index < probe_bytes; byte_index++) {
            if (pio_probe[byte_index] != irq_probe[byte_index]) {
                kernel_panic("ATA DMA/PIO data mismatch.");
            }
        }
        pmm_free_block(irq_probe_physical);
        pmm_free_block(pio_probe_physical);
        serial_write("EfesOS: ATA IRQ completion self-test passed.\n");
        if (ata_dma_mode_enabled() &&
            ata_dma_transfer_count() != dma_before) {
            serial_write("EfesOS: ATA DMA/PIO data integrity self-test passed.\n");
        }
    }
    serial_write("EfesOS: ATA IRQ mode enabled=");
    serial_write_hex((unsigned int)ata_irq_mode_enabled());
    serial_write(" irq-count=");
    serial_write_hex(ata_irq_count());
    serial_write(" polling-fallbacks=");
    serial_write_hex(ata_irq_fallback_count());
    serial_write(".\n");
    serial_write("EfesOS: ATA DMA mode enabled=");
    serial_write_hex((unsigned int)ata_dma_mode_enabled());
    serial_write(" transfer-mode=");
    serial_write_hex(ata_dma_transfer_mode());
    serial_write(" transfers=");
    serial_write_hex(ata_dma_transfer_count());
    serial_write(" fallbacks=");
    serial_write_hex(ata_dma_fallback_count());
    serial_write(".\n");
    syscall_init();
    __asm__ volatile ("int $0x03");
    __asm__ volatile ("int $0x30");
    {
        unsigned int syscall_ticks;
        unsigned int invalid_syscall;

        __asm__ volatile ("xor %%eax, %%eax\n\tint $0x80" : "=a"(syscall_ticks) : : "memory");
        __asm__ volatile ("mov $0xFFFFFFFF, %%eax\n\tint $0x80" : "=a"(invalid_syscall) : : "memory");
        if (syscall_ticks != pit_ticks() || invalid_syscall != SYSCALL_ENOSYS) {
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
    serial_write("EfesOS: paging mode=");
    serial_write(paging_uses_pae() ? "PAE" : "legacy");
    serial_write(" hardware-nx=");
    serial_write_hex((unsigned int)paging_uses_hardware_nx());
    serial_write(".\n");
    if (acpi_init(boot_info)) {
        serial_write("EfesOS: ACPI root table validated.\n");
        if (acpi_madt_available()) {
            const struct acpi_madt_info *madt = acpi_madt_get();

            serial_write("EfesOS: ACPI MADT validated lapic=");
            serial_write_hex(madt->local_apic_address);
            serial_write(" io-apics=");
            serial_write_hex(madt->io_apic_count);
            serial_write(" local-cpus=");
            serial_write_hex(madt->enabled_local_apics);
            serial_write(" x2-cpus=");
            serial_write_hex(madt->enabled_x2apics);
            serial_write(".\n");
            if (idt_enable_apic_routing(madt)) {
                serial_write("EfesOS: APIC interrupt routing enabled lapic-id=");
                serial_write_hex(idt_apic_id());
                serial_write(".\n");
            } else {
                serial_write("EfesOS: APIC routing unavailable; dual 8259 PIC fallback active.\n");
            }
        } else {
            serial_write("EfesOS: ACPI MADT unavailable or invalid.\n");
            serial_write("EfesOS: APIC routing unavailable; dual 8259 PIC fallback active.\n");
        }
        if (acpi_hpet_available()) {
            const struct acpi_hpet_info *hpet = acpi_hpet_get();

            serial_write("EfesOS: ACPI HPET table validated base=");
            serial_write_hex(hpet->physical_address);
            serial_write(" id=");
            serial_write_hex(hpet->event_timer_block_id);
            serial_write(" minimum-tick=");
            serial_write_hex(hpet->minimum_tick);
            serial_write(".\n");
            if (hpet_init(hpet) && hpet_self_test()) {
                serial_write("EfesOS: HPET monotonic counter self-test passed period-fs=");
                serial_write_hex(hpet_period_femtoseconds());
                serial_write(" width=");
                serial_write_hex(hpet_counter_is_64bit() ? 64U : 32U);
                serial_write(".\n");
            } else {
                serial_write("EfesOS: HPET monotonic clock unavailable; PIT fallback active.\n");
            }
        } else {
            serial_write("EfesOS: ACPI HPET table unavailable.\n");
            serial_write("EfesOS: HPET monotonic clock unavailable; PIT fallback active.\n");
        }
    } else {
        serial_write("EfesOS: ACPI root table unavailable or invalid.\n");
        serial_write("EfesOS: APIC routing unavailable; dual 8259 PIC fallback active.\n");
        serial_write("EfesOS: HPET monotonic clock unavailable; PIT fallback active.\n");
    }
    ahci_init();
    serial_write("EfesOS: AHCI disk present=");
    serial_write_hex((unsigned int)ahci_present());
    serial_write(" sectors=");
    serial_write_hex(ahci_sector_count());
    serial_write(" port=");
    serial_write_hex(ahci_port_number());
    serial_write(" version=");
    serial_write_hex(ahci_version());
    serial_write(" error=");
    serial_write_hex(ahci_last_error());
    serial_write(" readonly=0x00000001.\n");
    if (ahci_present()) {
        const struct block_device *ahci_device = ahci_block_device();
        unsigned char probe[BLOCK_DEVICE_SECTOR_SIZE];
        unsigned int reads_before = ahci_read_count();

        if (!block_device_is_ready(ahci_device) ||
            block_device_can_write(ahci_device) ||
            block_device_sector_count(ahci_device) != ahci_sector_count() ||
            !block_device_read(ahci_device, 0U, 1U, probe) ||
            ahci_read_count() != reads_before + 1U) {
            kernel_panic("AHCI read-only block device self-test failed.");
        }
        serial_write("EfesOS: AHCI read path self-test passed.\n");
        if (!ata_present()) {
            vfs_init(ahci_device);
            serial_write("EfesOS: AHCI FAT volume mounted=");
            serial_write_hex(vfs_is_mounted());
            serial_write(" error=");
            serial_write_hex(fat_last_error());
            serial_write(".\n");
            verify_mounted_disk_read();
        }
    }
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
    user_exit_runtime_verified = 0;
    user_pid_runtime_verified = 0;
    scheduler_stack_reap_runtime_verified = 0;
    scheduler_block_runtime_verified = 0;
    programs_init();
    ramfs_init();
    if (!persistent_ramfs_init()) {
        kernel_panic("Persistent journal initialization failed.");
    }
    if (persistent_ramfs_is_enabled()) {
        serial_write("EfesOS: persistent journal replay passed records=");
        serial_write_hex(persistent_ramfs_replay_count());
        serial_write(".\n");
    }
    if (!ramfs_self_test()) {
        kernel_panic("RAM filesystem self-test failed.");
    }
    serial_write("EfesOS: RAM filesystem self-test passed.\n");
    if (!journal_self_test()) {
        kernel_panic("Journal record self-test failed.");
    }
    serial_write("EfesOS: journal record self-test passed.\n");
    ipc_init();
    if (!ipc_self_test()) {
        kernel_panic("IPC queue self-test failed.");
    }
    serial_write("EfesOS: bounded IPC queue self-test passed.\n");
    scheduler_add_task("counter", counter_program);
    scheduler_add_task("snake", snake_program);
    if (!user_process_guard_self_test()) {
        kernel_panic("User stack guard self-test failed.");
    }
    serial_write("EfesOS: user stack guard self-test passed.\n");
    {
        unsigned int process_index;

        user_process_allocation_blocks = 0U;
        for (process_index = 0U; process_index < 4U; process_index++) {
            unsigned int free_before = pmm_free_blocks();
            unsigned int free_after;
            unsigned int consumed;

            if (!user_process_init()) {
                kernel_panic("User process initialization failed.");
            }
            free_after = pmm_free_blocks();
            if (free_after >= free_before) {
                kernel_panic("User process allocation accounting failed.");
            }
            consumed = free_before - free_after;
            if (process_index == 0U) {
                user_process_allocation_blocks = consumed;
            } else if (consumed != user_process_allocation_blocks) {
                kernel_panic("User process allocation cost is inconsistent.");
            }
        }
    }
    if (user_process_address_space() == 0U ||
        user_process_address_space() == paging_kernel_directory()) {
        kernel_panic("User process address-space isolation failed.");
    }
    if (user_process_active_count() != 4U ||
        user_process_address_space_at(0U) == 0U ||
        user_process_address_space_at(0U) == user_process_address_space_at(1U) ||
        user_process_address_space_at(0U) == user_process_address_space_at(2U) ||
        user_process_address_space_at(0U) == user_process_address_space_at(3U) ||
        user_process_address_space_at(1U) == user_process_address_space_at(2U) ||
        user_process_address_space_at(1U) == user_process_address_space_at(3U) ||
        user_process_address_space_at(2U) == user_process_address_space_at(3U) ||
        user_process_stack_address_at(0U) == 0U ||
        user_process_stack_address_at(0U) == user_process_stack_address_at(1U) ||
        user_process_stack_address_at(0U) == user_process_stack_address_at(2U) ||
        user_process_stack_address_at(0U) == user_process_stack_address_at(3U) ||
        user_process_stack_address_at(1U) == user_process_stack_address_at(2U) ||
        user_process_stack_address_at(1U) == user_process_stack_address_at(3U) ||
        user_process_stack_address_at(2U) == user_process_stack_address_at(3U)) {
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
    if (scheduler_add_user_task_in_space("invalid-unmapped-entry", 0x00500000U,
        0x00801000U, user_process_address_space())) {
        kernel_panic("Unmapped user entry accepted for user task.");
    }
    if (scheduler_add_user_task_in_space("invalid-unmapped-stack", 0x00400000U,
        0x00A00000U, user_process_address_space())) {
        kernel_panic("Unmapped user stack accepted for user task.");
    }
    if (scheduler_add_user_task_in_space("invalid-shared-space", 0x00400000U,
        0x00801000U, user_process_address_space())) {
        kernel_panic("User address space was shared between tasks.");
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
    if (!idt_enable_irq_line(1U)) {
        kernel_panic("Keyboard IRQ1 enable failed.");
    }
    vga_write("EfesOS: scheduler and game loop running.\n");
    shell_init();
    pit_init();
    if (idt_enable_apic_timer()) {
        serial_write("EfesOS: scheduler timer active=local-apic initial-count=");
        serial_write_hex(idt_apic_timer_initial_count());
        serial_write(".\n");
    } else {
        if (!idt_enable_irq_line(0U)) {
            kernel_panic("PIT IRQ0 enable failed.");
        }
        serial_write("EfesOS: scheduler timer active=pit.\n");
    }
    scheduler_start();

    for (;;) {
        __asm__ volatile ("sti\n\thlt" : : : "memory");
    }
}
