#include "ahci.h"
#include "ahci_layout.h"
#include "hpet.h"
#include "paging.h"
#include "pci.h"
#include "pit.h"
#include "pmm.h"

#define AHCI_MAP_VIRTUAL 0xCEC00000U
#define AHCI_DMA_MEMORY_LIMIT 0x00400000U
#define AHCI_GLOBAL_CAPABILITIES 0x000U
#define AHCI_GLOBAL_CONTROL 0x004U
#define AHCI_GLOBAL_INTERRUPT_STATUS 0x008U
#define AHCI_PORTS_IMPLEMENTED 0x00CU
#define AHCI_VERSION_REGISTER 0x010U
#define AHCI_CAPABILITIES_EXTENDED 0x024U
#define AHCI_BIOS_HANDOFF 0x028U
#define AHCI_PORT_BASE 0x100U
#define AHCI_PORT_STRIDE 0x080U
#define AHCI_PORT_COMMAND_LIST_BASE 0x000U
#define AHCI_PORT_COMMAND_LIST_BASE_UPPER 0x004U
#define AHCI_PORT_FIS_BASE 0x008U
#define AHCI_PORT_FIS_BASE_UPPER 0x00CU
#define AHCI_PORT_INTERRUPT_STATUS 0x010U
#define AHCI_PORT_INTERRUPT_ENABLE 0x014U
#define AHCI_PORT_COMMAND 0x018U
#define AHCI_PORT_TASK_FILE_DATA 0x020U
#define AHCI_PORT_SIGNATURE 0x024U
#define AHCI_PORT_SATA_STATUS 0x028U
#define AHCI_PORT_SATA_CONTROL 0x02CU
#define AHCI_PORT_SATA_ERROR 0x030U
#define AHCI_PORT_SATA_ACTIVE 0x034U
#define AHCI_PORT_COMMAND_ISSUE 0x038U
#define AHCI_GLOBAL_CONTROL_RESET 0x00000001U
#define AHCI_GLOBAL_CONTROL_INTERRUPT_ENABLE 0x00000002U
#define AHCI_GLOBAL_CONTROL_ENABLE 0x80000000U
#define AHCI_CAP2_BIOS_HANDOFF 0x00000001U
#define AHCI_BOHC_BIOS_OWNED 0x00000001U
#define AHCI_BOHC_OS_OWNED 0x00000002U
#define AHCI_BOHC_BIOS_BUSY 0x00000010U
#define AHCI_PORT_COMMAND_START 0x00000001U
#define AHCI_PORT_COMMAND_FIS_RECEIVE_ENABLE 0x00000010U
#define AHCI_PORT_COMMAND_FIS_RECEIVE_RUNNING 0x00004000U
#define AHCI_PORT_COMMAND_LIST_RUNNING 0x00008000U
#define AHCI_TASK_FILE_ERROR 0x00000001U
#define AHCI_TASK_FILE_DATA_REQUEST 0x00000008U
#define AHCI_TASK_FILE_DEVICE_FAULT 0x00000020U
#define AHCI_TASK_FILE_BUSY 0x00000080U
#define AHCI_PORT_INTERRUPT_ERROR_MASK 0x7D000000U
#define AHCI_WAIT_LIMIT 10000000U
#define AHCI_HANDOFF_WAIT_LIMIT 100000000U
#define AHCI_HANDOFF_TIMEOUT_NS 2000000000ULL
#define AHCI_RESET_ASSERT_MICROSECONDS 1000U
#define AHCI_RESET_LINK_TIMEOUT_MILLISECONDS 1000U
#define AHCI_RESET_ENGINE_TIMEOUT_MILLISECONDS 500U
#define AHCI_NANOSECONDS_PER_MICROSECOND 1000ULL

static volatile uint8_t *registers;
static volatile uint8_t *port_registers;
static const struct pci_device *controller;
static uint16_t original_pci_command;
static uint32_t mapped_physical_page;
static unsigned int mapped_page_count;
static uint32_t command_list_physical;
static uint32_t received_fis_physical;
static uint32_t command_table_physical;
static uint32_t bounce_physical;
static struct ahci_command_header *command_header;
static struct ahci_command_table *command_table;
static struct block_device block_device;
static uint32_t sectors;
static uint32_t controller_version;
static unsigned int selected_port;
static unsigned int error_code;
static unsigned int completed_reads;
static unsigned int recovery_attempts;
static unsigned int completed_recoveries;
static int supports_lba48;
static int device_present;
static int request_active;
static int mmio_control_safe;
static int fail_closed_state;
static uint16_t identify_baseline[256];

static uint32_t read_register(volatile uint8_t *base, unsigned int offset)
{
    volatile uint32_t *value = (volatile uint32_t *)(base + offset);
    uint32_t result = *value;

    __asm__ volatile ("" : : : "memory");
    return result;
}

static void write_register(volatile uint8_t *base, unsigned int offset,
    uint32_t value)
{
    volatile uint32_t *target = (volatile uint32_t *)(base + offset);

    __asm__ volatile ("" : : : "memory");
    *target = value;
    __asm__ volatile ("" : : : "memory");
}

static void clear_page(uint32_t physical)
{
    uint32_t *words = (uint32_t *)physical;
    unsigned int index;

    for (index = 0U; index < PAGE_SIZE / sizeof(uint32_t); index++) {
        words[index] = 0U;
    }
}

static void copy_bytes(void *destination, const void *source,
    unsigned int length)
{
    uint8_t *destination_bytes = (uint8_t *)destination;
    const uint8_t *source_bytes = (const uint8_t *)source;
    unsigned int index;

    for (index = 0U; index < length; index++) {
        destination_bytes[index] = source_bytes[index];
    }
}

static void copy_identify(uint16_t *destination, const uint16_t *source)
{
    unsigned int index;

    for (index = 0U; index < 256U; index++) {
        destination[index] = source[index];
    }
}

static unsigned int interrupt_save(void)
{
    unsigned int flags;

    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void interrupt_restore(unsigned int flags)
{
    if ((flags & (1U << 9U)) != 0U) {
        __asm__ volatile ("sti" : : : "memory");
    }
}

static int map_register_page(uint32_t physical_page, unsigned int index)
{
    uint32_t virtual_page = AHCI_MAP_VIRTUAL + index * PAGE_SIZE;

    if (paging_is_mapped(virtual_page) ||
        !paging_map_page(virtual_page, physical_page + index * PAGE_SIZE,
            PAGE_FLAG_WRITABLE | PAGE_FLAG_CACHE_DISABLE)) {
        return 0;
    }
    mapped_page_count = index + 1U;
    return 1;
}

static int map_register_space(uint32_t abar)
{
    uint32_t page_offset = abar & (PAGE_SIZE - 1U);
    uint32_t cap;
    uint32_t pi;
    uint32_t valid_mask;
    unsigned int highest_port;
    unsigned int required_bytes;
    unsigned int required_pages;
    unsigned int index;

    if (paging_current_directory() != paging_kernel_directory() ||
        abar < PAGE_SIZE || abar > 0xFFFFFFFFU -
            (AHCI_PORT_BASE + 32U * AHCI_PORT_STRIDE)) {
        return 0;
    }
    mapped_physical_page = abar & ~(PAGE_SIZE - 1U);
    if (!map_register_page(mapped_physical_page, 0U)) {
        return 0;
    }
    registers = (volatile uint8_t *)(AHCI_MAP_VIRTUAL + page_offset);
    cap = read_register(registers, AHCI_GLOBAL_CAPABILITIES);
    pi = read_register(registers, AHCI_PORTS_IMPLEMENTED);
    if (cap == 0xFFFFFFFFU || pi == 0U || pi == 0xFFFFFFFFU) {
        return 0;
    }
    highest_port = cap & 0x1FU;
    valid_mask = highest_port == 31U ? 0xFFFFFFFFU :
        (1U << (highest_port + 1U)) - 1U;
    if ((pi & ~valid_mask) != 0U) {
        return 0;
    }
    for (highest_port = 31U;
         highest_port != 0U && (pi & (1U << highest_port)) == 0U;
         highest_port--) {
    }
    required_bytes = page_offset + AHCI_PORT_BASE +
        (highest_port + 1U) * AHCI_PORT_STRIDE;
    required_pages = (required_bytes + PAGE_SIZE - 1U) / PAGE_SIZE;
    if (required_pages == 0U || required_pages > 3U ||
        mapped_physical_page > 0xFFFFFFFFU -
            (required_pages - 1U) * PAGE_SIZE) {
        return 0;
    }
    for (index = 1U; index < required_pages; index++) {
        if (!map_register_page(mapped_physical_page, index)) {
            return 0;
        }
    }
    return 1;
}

static void unmap_register_space(void)
{
    unsigned int index;

    for (index = 0U; index < mapped_page_count; index++) {
        (void)paging_unmap_page(AHCI_MAP_VIRTUAL + index * PAGE_SIZE);
    }
    mapped_page_count = 0U;
    mapped_physical_page = 0U;
    registers = 0;
    port_registers = 0;
}

static int acquire_bios_ownership(void)
{
    uint32_t start_low = 0U;
    uint32_t start_high = 0U;
    unsigned int attempt;
    int timed = hpet_available();

    if ((read_register(registers, AHCI_CAPABILITIES_EXTENDED) &
            AHCI_CAP2_BIOS_HANDOFF) == 0U) {
        return 1;
    }
    if (timed) {
        unsigned long long start = hpet_nanoseconds();

        start_low = (uint32_t)start;
        start_high = (uint32_t)(start >> 32U);
    }
    write_register(registers, AHCI_BIOS_HANDOFF,
        read_register(registers, AHCI_BIOS_HANDOFF) | AHCI_BOHC_OS_OWNED);
    for (attempt = 0U; attempt < AHCI_HANDOFF_WAIT_LIMIT; attempt++) {
        uint32_t ownership = read_register(registers, AHCI_BIOS_HANDOFF);

        if ((ownership & (AHCI_BOHC_BIOS_OWNED | AHCI_BOHC_BIOS_BUSY)) == 0U) {
            return (ownership & AHCI_BOHC_OS_OWNED) != 0U;
        }
        if (timed) {
            unsigned long long start = ((unsigned long long)start_high << 32U) |
                start_low;
            unsigned long long now = hpet_nanoseconds();

            if (now >= start && now - start >= AHCI_HANDOFF_TIMEOUT_NS) {
                return 0;
            }
        }
        __asm__ volatile ("pause");
    }
    return 0;
}

static int delay_microseconds(unsigned int microseconds)
{
    unsigned int attempt;

    if (!hpet_available()) {
        return pit_poll_delay_microseconds(microseconds);
    }
    {
        unsigned long long start = hpet_nanoseconds();
        unsigned long long duration =
            (unsigned long long)microseconds * AHCI_NANOSECONDS_PER_MICROSECOND;

        for (attempt = 0U; attempt < AHCI_HANDOFF_WAIT_LIMIT; attempt++) {
            unsigned long long now = hpet_nanoseconds();

            if (now < start) {
                return 0;
            }
            if (now - start >= duration) {
                return 1;
            }
            __asm__ volatile ("pause");
        }
    }
    return 0;
}

static int wait_port_bits_clear(unsigned int mask)
{
    unsigned int elapsed;

    for (elapsed = 0U; elapsed < AHCI_RESET_ENGINE_TIMEOUT_MILLISECONDS;
         elapsed++) {
        if ((read_register(port_registers, AHCI_PORT_COMMAND) & mask) == 0U) {
            return 1;
        }
        if (!delay_microseconds(1000U)) {
            return 0;
        }
    }
    return (read_register(port_registers, AHCI_PORT_COMMAND) & mask) == 0U;
}

static int stop_port(void)
{
    uint32_t command = read_register(port_registers, AHCI_PORT_COMMAND);

    write_register(port_registers, AHCI_PORT_COMMAND,
        command & ~AHCI_PORT_COMMAND_START);
    if (!wait_port_bits_clear(AHCI_PORT_COMMAND_LIST_RUNNING)) {
        return 0;
    }
    command = read_register(port_registers, AHCI_PORT_COMMAND);
    write_register(port_registers, AHCI_PORT_COMMAND,
        command & ~AHCI_PORT_COMMAND_FIS_RECEIVE_ENABLE);
    return wait_port_bits_clear(AHCI_PORT_COMMAND_FIS_RECEIVE_RUNNING);
}

static int start_port(void)
{
    uint32_t command;

    if (!wait_port_bits_clear(AHCI_PORT_COMMAND_LIST_RUNNING)) {
        return 0;
    }
    command = read_register(port_registers, AHCI_PORT_COMMAND);
    if ((command & AHCI_PORT_COMMAND_FIS_RECEIVE_ENABLE) == 0U) {
        if (!wait_port_bits_clear(AHCI_PORT_COMMAND_FIS_RECEIVE_RUNNING)) {
            return 0;
        }
        command |= AHCI_PORT_COMMAND_FIS_RECEIVE_ENABLE;
        write_register(port_registers, AHCI_PORT_COMMAND, command);
        if ((read_register(port_registers, AHCI_PORT_COMMAND) &
                AHCI_PORT_COMMAND_FIS_RECEIVE_ENABLE) == 0U) {
            return 0;
        }
    }
    write_register(port_registers, AHCI_PORT_COMMAND,
        command | AHCI_PORT_COMMAND_START);
    return (read_register(port_registers, AHCI_PORT_COMMAND) &
        AHCI_PORT_COMMAND_START) != 0U;
}

static int issue_slot_zero(unsigned int expected_bytes)
{
    unsigned int attempt;
    uint32_t interrupt_status;
    uint32_t task_file;

    if ((read_register(port_registers, AHCI_PORT_COMMAND_ISSUE) & 1U) != 0U ||
        (read_register(port_registers, AHCI_PORT_SATA_ACTIVE) & 1U) != 0U) {
        error_code = AHCI_ERROR_COMMAND_STATUS;
        return 0;
    }
    for (attempt = 0U; attempt < AHCI_WAIT_LIMIT; attempt++) {
        task_file = read_register(port_registers, AHCI_PORT_TASK_FILE_DATA);
        if ((task_file & (AHCI_TASK_FILE_BUSY |
                AHCI_TASK_FILE_DATA_REQUEST)) == 0U) {
            break;
        }
        __asm__ volatile ("pause");
    }
    if (attempt == AHCI_WAIT_LIMIT) {
        error_code = AHCI_ERROR_COMMAND_TIMEOUT;
        return 0;
    }

    write_register(port_registers, AHCI_PORT_INTERRUPT_STATUS, 0xFFFFFFFFU);
    write_register(port_registers, AHCI_PORT_SATA_ERROR, 0xFFFFFFFFU);
    write_register(registers, AHCI_GLOBAL_INTERRUPT_STATUS,
        1U << selected_port);
    __asm__ volatile ("" : : : "memory");
    write_register(port_registers, AHCI_PORT_COMMAND_ISSUE, 1U);
    for (attempt = 0U; attempt < AHCI_WAIT_LIMIT; attempt++) {
        interrupt_status = read_register(port_registers,
            AHCI_PORT_INTERRUPT_STATUS);
        if ((interrupt_status & AHCI_PORT_INTERRUPT_ERROR_MASK) != 0U) {
            error_code = AHCI_ERROR_COMMAND_STATUS;
            return 0;
        }
        if ((read_register(port_registers, AHCI_PORT_COMMAND_ISSUE) & 1U) == 0U) {
            break;
        }
        __asm__ volatile ("pause");
    }
    if (attempt == AHCI_WAIT_LIMIT) {
        error_code = AHCI_ERROR_COMMAND_TIMEOUT;
        return 0;
    }
    __asm__ volatile ("" : : : "memory");
    interrupt_status = read_register(port_registers,
        AHCI_PORT_INTERRUPT_STATUS);
    task_file = read_register(port_registers, AHCI_PORT_TASK_FILE_DATA);
    if ((interrupt_status & AHCI_PORT_INTERRUPT_ERROR_MASK) != 0U ||
        (task_file & (AHCI_TASK_FILE_ERROR | AHCI_TASK_FILE_DEVICE_FAULT |
            AHCI_TASK_FILE_BUSY | AHCI_TASK_FILE_DATA_REQUEST)) != 0U) {
        error_code = AHCI_ERROR_COMMAND_STATUS;
        return 0;
    }
    if (command_header->prd_byte_count != expected_bytes) {
        error_code = AHCI_ERROR_TRANSFER_COUNT;
        return 0;
    }
    write_register(port_registers, AHCI_PORT_INTERRUPT_STATUS, 0xFFFFFFFFU);
    write_register(registers, AHCI_GLOBAL_INTERRUPT_STATUS,
        1U << selected_port);
    return 1;
}

static int reset_port_link(void)
{
    uint32_t command;
    uint32_t sata_control;
    unsigned int elapsed;

    recovery_attempts++;
    command = read_register(port_registers, AHCI_PORT_COMMAND);
    write_register(port_registers, AHCI_PORT_COMMAND,
        command & ~AHCI_PORT_COMMAND_START);
    (void)wait_port_bits_clear(AHCI_PORT_COMMAND_LIST_RUNNING);

    sata_control = read_register(port_registers, AHCI_PORT_SATA_CONTROL);
    write_register(port_registers, AHCI_PORT_SATA_CONTROL,
        ahci_comreset_assert_control(sata_control));
    if ((read_register(port_registers, AHCI_PORT_SATA_CONTROL) &
            AHCI_SCTL_DETECTION_MASK) != AHCI_SCTL_DETECTION_COMRESET ||
        !delay_microseconds(AHCI_RESET_ASSERT_MICROSECONDS)) {
        error_code = AHCI_ERROR_RESET_DELAY;
        return 0;
    }
    write_register(port_registers, AHCI_PORT_SATA_CONTROL,
        ahci_comreset_release_control(sata_control));
    if ((read_register(port_registers, AHCI_PORT_SATA_CONTROL) &
            AHCI_SCTL_DETECTION_MASK) != 0U) {
        error_code = AHCI_ERROR_RESET_LINK;
        return 0;
    }

    for (elapsed = 0U; elapsed < AHCI_RESET_LINK_TIMEOUT_MILLISECONDS;
         elapsed++) {
        if (ahci_link_is_established(
                read_register(port_registers, AHCI_PORT_SATA_STATUS),
                read_register(port_registers, AHCI_PORT_SIGNATURE))) {
            break;
        }
        if (!delay_microseconds(1000U)) {
            error_code = AHCI_ERROR_RESET_DELAY;
            return 0;
        }
    }
    if (elapsed == AHCI_RESET_LINK_TIMEOUT_MILLISECONDS) {
        error_code = AHCI_ERROR_RESET_LINK;
        return 0;
    }

    write_register(port_registers, AHCI_PORT_SATA_ERROR, 0xFFFFFFFFU);
    write_register(port_registers, AHCI_PORT_INTERRUPT_STATUS, 0xFFFFFFFFU);
    write_register(registers, AHCI_GLOBAL_INTERRUPT_STATUS,
        1U << selected_port);
    if (!wait_port_bits_clear(AHCI_PORT_COMMAND_LIST_RUNNING) ||
        (read_register(port_registers, AHCI_PORT_COMMAND_ISSUE) & 1U) != 0U ||
        (read_register(port_registers, AHCI_PORT_SATA_ACTIVE) & 1U) != 0U ||
        !start_port()) {
        error_code = AHCI_ERROR_RESET_ENGINE;
        return 0;
    }
    return 1;
}

static int recover_and_revalidate(void)
{
    uint16_t *identify = (uint16_t *)bounce_physical;

    if (!reset_port_link() ||
        !ahci_build_identify_command(command_header, command_table,
            command_table_physical, bounce_physical) ||
        !issue_slot_zero(AHCI_SECTOR_SIZE)) {
        return 0;
    }
    if (!ahci_identify_same_device(identify_baseline, identify)) {
        error_code = AHCI_ERROR_RESET_IDENTITY;
        return 0;
    }
    completed_recoveries++;
    error_code = AHCI_ERROR_NONE;
    return 1;
}

static void release_dma_pages(void)
{
    if (bounce_physical != 0U) {
        pmm_free_block(bounce_physical);
    }
    if (command_table_physical != 0U) {
        pmm_free_block(command_table_physical);
    }
    if (received_fis_physical != 0U) {
        pmm_free_block(received_fis_physical);
    }
    if (command_list_physical != 0U) {
        pmm_free_block(command_list_physical);
    }
    bounce_physical = 0U;
    command_table_physical = 0U;
    received_fis_physical = 0U;
    command_list_physical = 0U;
    command_header = 0;
    command_table = 0;
}

static void disable_driver(void)
{
    int port_stopped = 1;
    int bus_master_stopped = 1;

    block_device_reset(&block_device);
    device_present = 0;
    sectors = 0U;
    supports_lba48 = 0;
    if (mmio_control_safe && port_registers != 0) {
        write_register(port_registers, AHCI_PORT_INTERRUPT_ENABLE, 0U);
        port_stopped = stop_port();
        if (port_stopped) {
            write_register(port_registers, AHCI_PORT_COMMAND_LIST_BASE, 0U);
            write_register(port_registers, AHCI_PORT_COMMAND_LIST_BASE_UPPER, 0U);
            write_register(port_registers, AHCI_PORT_FIS_BASE, 0U);
            write_register(port_registers, AHCI_PORT_FIS_BASE_UPPER, 0U);
        }
    }
    if (mmio_control_safe && registers != 0) {
        write_register(registers, AHCI_GLOBAL_CONTROL,
            read_register(registers, AHCI_GLOBAL_CONTROL) &
                ~AHCI_GLOBAL_CONTROL_INTERRUPT_ENABLE);
    }
    if (controller != 0) {
        bus_master_stopped = pci_quiesce_ahci_controller(controller,
            original_pci_command);
    }
    if (bus_master_stopped && port_stopped) {
        release_dma_pages();
        unmap_register_space();
    }
    fail_closed_state = bus_master_stopped && port_stopped;
    if (!port_stopped && error_code == AHCI_ERROR_NONE) {
        error_code = AHCI_ERROR_PORT_STOP;
    }
}

static int ahci_block_read(void *context, unsigned int lba,
    unsigned char count, void *buffer)
{
    unsigned int flags;
    unsigned int byte_count;
    int success = 0;

    (void)context;
    if (!device_present || buffer == 0 || count == 0U ||
        count > AHCI_MAX_TRANSFER_SECTORS || lba >= sectors ||
        (unsigned int)count > sectors - lba) {
        return 0;
    }
    flags = interrupt_save();
    if (request_active) {
        interrupt_restore(flags);
        return 0;
    }
    request_active = 1;
    byte_count = (unsigned int)count * AHCI_SECTOR_SIZE;
    if (ahci_build_read_command(command_header, command_table,
            command_table_physical, bounce_physical, lba, count,
            supports_lba48) && issue_slot_zero(byte_count)) {
        copy_bytes(buffer, (const void *)bounce_physical, byte_count);
        completed_reads++;
        success = 1;
    } else if (recover_and_revalidate() &&
        ahci_build_read_command(command_header, command_table,
            command_table_physical, bounce_physical, lba, count,
            supports_lba48) && issue_slot_zero(byte_count)) {
        copy_bytes(buffer, (const void *)bounce_physical, byte_count);
        completed_reads++;
        success = 1;
    } else {
        disable_driver();
    }
    request_active = 0;
    interrupt_restore(flags);
    return success;
}

void ahci_init(void)
{
    uint32_t abar;
    uint32_t implemented_ports;
    uint32_t global_control;
    uint16_t *identify;
    unsigned int port;

    registers = 0;
    port_registers = 0;
    controller = 0;
    original_pci_command = 0U;
    mapped_physical_page = 0U;
    mapped_page_count = 0U;
    command_list_physical = 0U;
    received_fis_physical = 0U;
    command_table_physical = 0U;
    bounce_physical = 0U;
    command_header = 0;
    command_table = 0;
    sectors = 0U;
    controller_version = 0U;
    selected_port = 0U;
    error_code = AHCI_ERROR_NO_CONTROLLER;
    completed_reads = 0U;
    recovery_attempts = 0U;
    completed_recoveries = 0U;
    supports_lba48 = 0;
    device_present = 0;
    request_active = 0;
    mmio_control_safe = 0;
    fail_closed_state = 0;
    block_device_reset(&block_device);

    controller = pci_ahci_device_at(0U);
    if (controller == 0 || !pci_ahci_mmio_base(controller, &abar)) {
        return;
    }
    error_code = AHCI_ERROR_PCI_COMMAND;
    if (!pci_prepare_ahci_controller(controller, &original_pci_command)) {
        (void)pci_quiesce_ahci_controller(controller, original_pci_command);
        return;
    }
    error_code = AHCI_ERROR_MMIO_MAPPING;
    if (!map_register_space(abar)) {
        disable_driver();
        return;
    }
    implemented_ports = read_register(registers, AHCI_PORTS_IMPLEMENTED);
    controller_version = read_register(registers, AHCI_VERSION_REGISTER);
    global_control = read_register(registers, AHCI_GLOBAL_CONTROL);
    error_code = AHCI_ERROR_CAPABILITIES;
    if (controller_version == 0U || controller_version == 0xFFFFFFFFU ||
        (controller_version >> 16U) == 0U ||
        (global_control & AHCI_GLOBAL_CONTROL_RESET) != 0U) {
        disable_driver();
        return;
    }
    mmio_control_safe = 1;
    error_code = AHCI_ERROR_BIOS_HANDOFF;
    if (!acquire_bios_ownership()) {
        disable_driver();
        return;
    }
    global_control = read_register(registers, AHCI_GLOBAL_CONTROL);
    if ((global_control & AHCI_GLOBAL_CONTROL_RESET) != 0U) {
        error_code = AHCI_ERROR_CAPABILITIES;
        disable_driver();
        return;
    }
    write_register(registers, AHCI_GLOBAL_CONTROL,
        (global_control | AHCI_GLOBAL_CONTROL_ENABLE) &
            ~AHCI_GLOBAL_CONTROL_INTERRUPT_ENABLE);
    if ((read_register(registers, AHCI_GLOBAL_CONTROL) &
            (AHCI_GLOBAL_CONTROL_ENABLE |
             AHCI_GLOBAL_CONTROL_INTERRUPT_ENABLE)) !=
            AHCI_GLOBAL_CONTROL_ENABLE) {
        error_code = AHCI_ERROR_CAPABILITIES;
        disable_driver();
        return;
    }

    error_code = AHCI_ERROR_NO_SATA_DEVICE;
    for (port = 0U; port < 32U; port++) {
        volatile uint8_t *candidate;

        if ((implemented_ports & (1U << port)) == 0U) {
            continue;
        }
        candidate = registers + AHCI_PORT_BASE + port * AHCI_PORT_STRIDE;
        if (ahci_port_is_usable_sata(implemented_ports, port,
                read_register(candidate, AHCI_PORT_SATA_STATUS),
                read_register(candidate, AHCI_PORT_SIGNATURE))) {
            selected_port = port;
            port_registers = candidate;
            break;
        }
    }
    if (port_registers == 0) {
        disable_driver();
        return;
    }

    error_code = AHCI_ERROR_MEMORY;
    command_list_physical = pmm_alloc_block_below(AHCI_DMA_MEMORY_LIMIT);
    received_fis_physical = pmm_alloc_block_below(AHCI_DMA_MEMORY_LIMIT);
    command_table_physical = pmm_alloc_block_below(AHCI_DMA_MEMORY_LIMIT);
    bounce_physical = pmm_alloc_block_below(AHCI_DMA_MEMORY_LIMIT);
    if (command_list_physical == 0U || received_fis_physical == 0U ||
        command_table_physical == 0U || bounce_physical == 0U) {
        disable_driver();
        return;
    }
    clear_page(command_list_physical);
    clear_page(received_fis_physical);
    clear_page(command_table_physical);
    clear_page(bounce_physical);
    command_header = (struct ahci_command_header *)command_list_physical;
    command_table = (struct ahci_command_table *)command_table_physical;

    error_code = AHCI_ERROR_PORT_STOP;
    if (!stop_port()) {
        disable_driver();
        return;
    }
    write_register(port_registers, AHCI_PORT_INTERRUPT_ENABLE, 0U);
    write_register(port_registers, AHCI_PORT_INTERRUPT_STATUS, 0xFFFFFFFFU);
    write_register(port_registers, AHCI_PORT_SATA_ERROR, 0xFFFFFFFFU);
    write_register(port_registers, AHCI_PORT_COMMAND_LIST_BASE,
        command_list_physical);
    write_register(port_registers, AHCI_PORT_COMMAND_LIST_BASE_UPPER, 0U);
    write_register(port_registers, AHCI_PORT_FIS_BASE, received_fis_physical);
    write_register(port_registers, AHCI_PORT_FIS_BASE_UPPER, 0U);
    if (read_register(port_registers, AHCI_PORT_COMMAND_LIST_BASE) !=
            command_list_physical ||
        read_register(port_registers, AHCI_PORT_COMMAND_LIST_BASE_UPPER) != 0U ||
        read_register(port_registers, AHCI_PORT_FIS_BASE) !=
            received_fis_physical ||
        read_register(port_registers, AHCI_PORT_FIS_BASE_UPPER) != 0U) {
        error_code = AHCI_ERROR_CAPABILITIES;
        disable_driver();
        return;
    }
    if (!pci_enable_ahci_bus_master(controller)) {
        error_code = AHCI_ERROR_PCI_COMMAND;
        disable_driver();
        return;
    }
    error_code = AHCI_ERROR_PORT_START;
    if (!start_port()) {
        disable_driver();
        return;
    }
    error_code = AHCI_ERROR_IDENTIFY;
    if (!ahci_build_identify_command(command_header, command_table,
            command_table_physical, bounce_physical) ||
        !issue_slot_zero(AHCI_SECTOR_SIZE)) {
        disable_driver();
        return;
    }
    identify = (uint16_t *)bounce_physical;
    if (!ahci_identify_capacity(identify, &sectors, &supports_lba48)) {
        error_code = AHCI_ERROR_IDENTIFY;
        disable_driver();
        return;
    }
    copy_identify(identify_baseline, identify);
    if (!block_device_configure(&block_device, sectors, AHCI_SECTOR_SIZE,
            AHCI_MAX_TRANSFER_SECTORS, ahci_block_read, 0, 0)) {
        error_code = AHCI_ERROR_BLOCK_DEVICE;
        disable_driver();
        return;
    }
    error_code = AHCI_ERROR_NONE;
    device_present = 1;
}

int ahci_present(void)
{
    return device_present;
}

unsigned int ahci_sector_count(void)
{
    return device_present ? sectors : 0U;
}

unsigned int ahci_port_number(void)
{
    return selected_port;
}

unsigned int ahci_version(void)
{
    return controller_version;
}

unsigned int ahci_last_error(void)
{
    return error_code;
}

unsigned int ahci_read_count(void)
{
    return completed_reads;
}

unsigned int ahci_recovery_count(void)
{
    return completed_recoveries;
}

unsigned int ahci_recovery_attempt_count(void)
{
    return recovery_attempts;
}

int ahci_fail_closed(void)
{
    return fail_closed_state;
}

const struct block_device *ahci_block_device(void)
{
    return block_device_is_ready(&block_device) ? &block_device : 0;
}
