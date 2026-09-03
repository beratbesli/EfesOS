#include "apic.h"
#include "features.h"
#include "paging.h"

#define IA32_APIC_BASE_MSR 0x01BU
#define IA32_APIC_BASE_ENABLE (1U << 11U)
#define IA32_APIC_BASE_X2APIC (1U << 10U)
#define IA32_APIC_BASE_MASK 0xFFFFF000U

#define LOCAL_APIC_VIRTUAL 0xCEE00000U
#define IO_APIC_VIRTUAL_BASE 0xCED00000U
#define LOCAL_APIC_ID 0x020U
#define LOCAL_APIC_VERSION 0x030U
#define LOCAL_APIC_TASK_PRIORITY 0x080U
#define LOCAL_APIC_EOI 0x0B0U
#define LOCAL_APIC_SPURIOUS 0x0F0U
#define LOCAL_APIC_LVT_TIMER 0x320U
#define LOCAL_APIC_LVT_LINT0 0x350U
#define LOCAL_APIC_LVT_LINT1 0x360U
#define LOCAL_APIC_LVT_ERROR 0x370U
#define LOCAL_APIC_SOFTWARE_ENABLE (1U << 8U)
#define LOCAL_APIC_LVT_MASKED (1U << 16U)
#define LOCAL_APIC_DELIVERY_MASK (7U << 8U)
#define LOCAL_APIC_DELIVERY_NMI (4U << 8U)

#define IO_APIC_SELECTOR 0x00U
#define IO_APIC_WINDOW 0x10U
#define IO_APIC_ID_REGISTER 0x00U
#define IO_APIC_VERSION_REGISTER 0x01U
#define IO_APIC_REDIRECTION_BASE 0x10U
#define IO_APIC_MAX_REDIRECTIONS 120U
#define IO_APIC_POLARITY_LOW (1U << 13U)
#define IO_APIC_TRIGGER_LEVEL (1U << 15U)
#define IO_APIC_MASKED (1U << 16U)
#define IO_APIC_REDIRECTION_COMPARE_MASK 0x0001AFFFU

#define LEGACY_IRQ_BASE 32U
#define SUPPORTED_IRQ_BITMAP ((1U << 0U) | (1U << 1U) | (1U << 14U))

struct io_apic_runtime {
    volatile unsigned char *registers;
    unsigned int physical_address;
    unsigned int id;
    unsigned int global_interrupt_base;
    unsigned int redirection_count;
    unsigned int snapshot_count;
    unsigned int saved_low[IO_APIC_MAX_REDIRECTIONS];
    unsigned int saved_high[IO_APIC_MAX_REDIRECTIONS];
};

static volatile unsigned char *local_apic_registers;
static struct io_apic_runtime io_apics[ACPI_MAX_IO_APICS];
static const struct acpi_madt_info *firmware_madt;
static unsigned int io_apic_count;
static unsigned int enabled_irq_bitmap;
static unsigned int current_local_apic_id;
static unsigned int saved_task_priority;
static unsigned int saved_spurious;
static unsigned int saved_lvt_timer;
static unsigned int saved_lvt_lint0;
static unsigned int saved_lvt_lint1;
static unsigned int saved_lvt_error;
static int local_state_saved;
static int initialized;
static int available;

static void read_msr(unsigned int index, unsigned int *low, unsigned int *high)
{
    __asm__ volatile ("rdmsr" : "=a"(*low), "=d"(*high) : "c"(index));
}

static unsigned int local_read(unsigned int offset)
{
    volatile unsigned int *reg = (volatile unsigned int *)(
        local_apic_registers + offset);
    unsigned int value = *reg;

    __asm__ volatile ("" : : : "memory");
    return value;
}

static void local_write(unsigned int offset, unsigned int value)
{
    volatile unsigned int *reg = (volatile unsigned int *)(
        local_apic_registers + offset);

    __asm__ volatile ("" : : : "memory");
    *reg = value;
    __asm__ volatile ("" : : : "memory");
}

static unsigned int io_read(struct io_apic_runtime *io, unsigned int reg)
{
    volatile unsigned int *selector = (volatile unsigned int *)(
        io->registers + IO_APIC_SELECTOR);
    volatile unsigned int *window = (volatile unsigned int *)(
        io->registers + IO_APIC_WINDOW);

    *selector = reg;
    __asm__ volatile ("" : : : "memory");
    return *window;
}

static void io_write(struct io_apic_runtime *io, unsigned int reg,
    unsigned int value)
{
    volatile unsigned int *selector = (volatile unsigned int *)(
        io->registers + IO_APIC_SELECTOR);
    volatile unsigned int *window = (volatile unsigned int *)(
        io->registers + IO_APIC_WINDOW);

    *selector = reg;
    __asm__ volatile ("" : : : "memory");
    *window = value;
    __asm__ volatile ("" : : : "memory");
}

static unsigned int redirection_low_register(unsigned int entry)
{
    return IO_APIC_REDIRECTION_BASE + entry * 2U;
}

static unsigned int redirection_high_register(unsigned int entry)
{
    return redirection_low_register(entry) + 1U;
}

static int local_id_is_described(unsigned int id,
    const struct acpi_madt_info *madt)
{
    return id < 256U && (madt->local_apic_id_bitmap[id >> 5U] &
        (1U << (id & 31U))) != 0U;
}

static unsigned int configured_lint(unsigned int saved)
{
    if ((saved & LOCAL_APIC_DELIVERY_MASK) == LOCAL_APIC_DELIVERY_NMI) {
        return saved & ~LOCAL_APIC_LVT_MASKED;
    }
    return saved | LOCAL_APIC_LVT_MASKED;
}

static int initialize_local_apic(const struct acpi_madt_info *madt)
{
    const struct cpu_features *features = cpu_features_get();
    unsigned int apic_base_low;
    unsigned int apic_base_high;
    unsigned int version;
    unsigned int spurious;

    if (features == 0 || features->cpuid == 0U || features->msr == 0U ||
        features->apic == 0U || madt == 0 ||
        madt->local_apic_address < PAGE_SIZE ||
        (madt->local_apic_address & (PAGE_SIZE - 1U)) != 0U ||
        paging_current_directory() != paging_kernel_directory()) {
        return 0;
    }
    read_msr(IA32_APIC_BASE_MSR, &apic_base_low, &apic_base_high);
    if ((apic_base_low & IA32_APIC_BASE_ENABLE) == 0U ||
        (apic_base_low & IA32_APIC_BASE_X2APIC) != 0U ||
        apic_base_high != 0U ||
        (apic_base_low & IA32_APIC_BASE_MASK) != madt->local_apic_address ||
        paging_is_mapped(LOCAL_APIC_VIRTUAL) ||
        !paging_map_page(LOCAL_APIC_VIRTUAL, madt->local_apic_address,
            PAGE_FLAG_WRITABLE | PAGE_FLAG_CACHE_DISABLE)) {
        return 0;
    }
    local_apic_registers = (volatile unsigned char *)LOCAL_APIC_VIRTUAL;
    version = local_read(LOCAL_APIC_VERSION);
    current_local_apic_id = local_read(LOCAL_APIC_ID) >> 24U;
    if ((version & 0xFFU) == 0U || (version & 0xFFU) == 0xFFU ||
        ((version >> 16U) & 0xFFU) < 3U ||
        !local_id_is_described(current_local_apic_id, madt)) {
        paging_unmap_page(LOCAL_APIC_VIRTUAL);
        local_apic_registers = 0;
        return 0;
    }

    saved_task_priority = local_read(LOCAL_APIC_TASK_PRIORITY);
    saved_spurious = local_read(LOCAL_APIC_SPURIOUS);
    saved_lvt_timer = local_read(LOCAL_APIC_LVT_TIMER);
    saved_lvt_lint0 = local_read(LOCAL_APIC_LVT_LINT0);
    saved_lvt_lint1 = local_read(LOCAL_APIC_LVT_LINT1);
    saved_lvt_error = local_read(LOCAL_APIC_LVT_ERROR);
    local_state_saved = 1;

    local_write(LOCAL_APIC_TASK_PRIORITY, 0U);
    local_write(LOCAL_APIC_LVT_TIMER,
        saved_lvt_timer | LOCAL_APIC_LVT_MASKED);
    local_write(LOCAL_APIC_LVT_LINT0, configured_lint(saved_lvt_lint0));
    local_write(LOCAL_APIC_LVT_LINT1, configured_lint(saved_lvt_lint1));
    local_write(LOCAL_APIC_LVT_ERROR,
        saved_lvt_error | LOCAL_APIC_LVT_MASKED);
    spurious = (saved_spurious & ~0xFFU) | APIC_SPURIOUS_VECTOR |
        LOCAL_APIC_SOFTWARE_ENABLE;
    local_write(LOCAL_APIC_SPURIOUS, spurious);
    return local_read(LOCAL_APIC_TASK_PRIORITY) == 0U &&
        (local_read(LOCAL_APIC_SPURIOUS) &
        (LOCAL_APIC_SOFTWARE_ENABLE | 0xFFU)) ==
        (LOCAL_APIC_SOFTWARE_ENABLE | APIC_SPURIOUS_VECTOR);
}

static int map_and_mask_io_apic(const struct acpi_io_apic_info *firmware,
    unsigned int index)
{
    struct io_apic_runtime *io = &io_apics[index];
    unsigned int virtual_address = IO_APIC_VIRTUAL_BASE + index * PAGE_SIZE;
    unsigned int id_register;
    unsigned int version_register;
    unsigned int entry;

    if (firmware == 0 || firmware->physical_address < PAGE_SIZE ||
        (firmware->physical_address & (PAGE_SIZE - 1U)) != 0U ||
        paging_is_mapped(virtual_address) ||
        !paging_map_page(virtual_address, firmware->physical_address,
            PAGE_FLAG_WRITABLE | PAGE_FLAG_CACHE_DISABLE)) {
        return 0;
    }
    io->registers = (volatile unsigned char *)virtual_address;
    io->physical_address = firmware->physical_address;
    io->id = firmware->id;
    io->global_interrupt_base = firmware->global_interrupt_base;
    io->redirection_count = 0U;
    io->snapshot_count = 0U;
    io_apic_count = index + 1U;

    id_register = io_read(io, IO_APIC_ID_REGISTER);
    version_register = io_read(io, IO_APIC_VERSION_REGISTER);
    io->redirection_count = ((version_register >> 16U) & 0xFFU) + 1U;
    if (((id_register >> 24U) & 0xFFU) != io->id ||
        (version_register & 0xFFU) == 0U ||
        (version_register & 0xFFU) == 0xFFU ||
        io->redirection_count == 0U ||
        io->redirection_count > IO_APIC_MAX_REDIRECTIONS ||
        io->global_interrupt_base >
            0xFFFFFFFFU - (io->redirection_count - 1U)) {
        return 0;
    }
    for (entry = 0U; entry < io->redirection_count; entry++) {
        unsigned int low_reg = redirection_low_register(entry);

        io->saved_low[entry] = io_read(io, low_reg);
        io->saved_high[entry] = io_read(io,
            redirection_high_register(entry));
        io->snapshot_count = entry + 1U;
        io_write(io, low_reg, io->saved_low[entry] | IO_APIC_MASKED);
        if ((io_read(io, low_reg) & IO_APIC_MASKED) == 0U) {
            return 0;
        }
    }
    return 1;
}

static int io_apic_ranges_valid(void)
{
    unsigned int left;

    for (left = 0U; left < io_apic_count; left++) {
        unsigned int left_start = io_apics[left].global_interrupt_base;
        unsigned int left_end = left_start +
            io_apics[left].redirection_count - 1U;
        unsigned int right;

        for (right = left + 1U; right < io_apic_count; right++) {
            unsigned int right_start = io_apics[right].global_interrupt_base;
            unsigned int right_end = right_start +
                io_apics[right].redirection_count - 1U;

            if (left_start <= right_end && right_start <= left_end) {
                return 0;
            }
        }
    }
    return 1;
}

static struct io_apic_runtime *io_apic_for_gsi(unsigned int gsi,
    unsigned int *entry)
{
    unsigned int index;

    for (index = 0U; index < io_apic_count; index++) {
        struct io_apic_runtime *io = &io_apics[index];

        if (gsi >= io->global_interrupt_base &&
            gsi - io->global_interrupt_base < io->redirection_count) {
            if (entry != 0) {
                *entry = gsi - io->global_interrupt_base;
            }
            return io;
        }
    }
    return 0;
}

static void restore_io_apics(void)
{
    unsigned int io_index;

    for (io_index = 0U; io_index < io_apic_count; io_index++) {
        struct io_apic_runtime *io = &io_apics[io_index];
        unsigned int entry;

        if (io->registers == 0) {
            continue;
        }
        for (entry = 0U; entry < io->snapshot_count; entry++) {
            unsigned int low_reg = redirection_low_register(entry);

            io_write(io, low_reg, io->saved_low[entry] | IO_APIC_MASKED);
            io_write(io, redirection_high_register(entry),
                io->saved_high[entry]);
            io_write(io, low_reg, io->saved_low[entry]);
        }
    }
}

static void unmap_apics(void)
{
    unsigned int index;

    for (index = 0U; index < io_apic_count; index++) {
        if (io_apics[index].registers != 0) {
            paging_unmap_page(IO_APIC_VIRTUAL_BASE + index * PAGE_SIZE);
            io_apics[index].registers = 0;
        }
    }
    io_apic_count = 0U;
    if (local_apic_registers != 0) {
        paging_unmap_page(LOCAL_APIC_VIRTUAL);
        local_apic_registers = 0;
    }
}

int apic_init(const struct acpi_madt_info *madt)
{
    unsigned int index;

    if (initialized) {
        return available;
    }
    initialized = 1;
    available = 0;
    enabled_irq_bitmap = 0U;
    firmware_madt = 0;
    io_apic_count = 0U;
    local_state_saved = 0;
    if (madt == 0 || madt->io_apic_count == 0U ||
        madt->io_apic_count > ACPI_MAX_IO_APICS ||
        !initialize_local_apic(madt)) {
        apic_shutdown();
        return 0;
    }
    for (index = 0U; index < madt->io_apic_count; index++) {
        if (!map_and_mask_io_apic(&madt->io_apics[index], index)) {
            apic_shutdown();
            return 0;
        }
    }
    if (!io_apic_ranges_valid()) {
        apic_shutdown();
        return 0;
    }
    firmware_madt = madt;
    available = 1;
    return 1;
}

void apic_shutdown(void)
{
    available = 0;
    enabled_irq_bitmap = 0U;
    restore_io_apics();
    if (local_apic_registers != 0 && local_state_saved) {
        local_write(LOCAL_APIC_LVT_TIMER, saved_lvt_timer);
        local_write(LOCAL_APIC_LVT_LINT0, saved_lvt_lint0);
        local_write(LOCAL_APIC_LVT_LINT1, saved_lvt_lint1);
        local_write(LOCAL_APIC_LVT_ERROR, saved_lvt_error);
        local_write(LOCAL_APIC_TASK_PRIORITY, saved_task_priority);
        local_write(LOCAL_APIC_SPURIOUS, saved_spurious);
    }
    local_state_saved = 0;
    firmware_madt = 0;
    unmap_apics();
}

int apic_available(void)
{
    return initialized && available;
}

int apic_enable_irq(unsigned int irq)
{
    const struct acpi_irq_override_info *override;
    struct io_apic_runtime *io;
    unsigned int gsi;
    unsigned int flags;
    unsigned int entry;
    unsigned int low;
    unsigned int high;
    unsigned int low_reg;

    if (!apic_available() || irq >= ACPI_ISA_IRQ_COUNT ||
        (SUPPORTED_IRQ_BITMAP & (1U << irq)) == 0U) {
        return 0;
    }
    override = &firmware_madt->isa_overrides[irq];
    gsi = override->present != 0U ? override->global_interrupt : irq;
    flags = override->present != 0U ? override->flags : 0U;
    io = io_apic_for_gsi(gsi, &entry);
    if (io == 0) {
        return 0;
    }
    low = LEGACY_IRQ_BASE + irq;
    if ((flags & 3U) == 3U) {
        low |= IO_APIC_POLARITY_LOW;
    }
    if (((flags >> 2U) & 3U) == 3U) {
        low |= IO_APIC_TRIGGER_LEVEL;
    }
    high = current_local_apic_id << 24U;
    low_reg = redirection_low_register(entry);
    io_write(io, low_reg, low | IO_APIC_MASKED);
    io_write(io, redirection_high_register(entry), high);
    io_write(io, low_reg, low);
    if ((io_read(io, low_reg) & IO_APIC_REDIRECTION_COMPARE_MASK) != low ||
        (io_read(io, redirection_high_register(entry)) & 0xFF000000U) !=
            high) {
        io_write(io, low_reg, low | IO_APIC_MASKED);
        return 0;
    }
    enabled_irq_bitmap |= 1U << irq;
    return 1;
}

int apic_irq_enabled(unsigned int irq)
{
    const struct acpi_irq_override_info *override;
    struct io_apic_runtime *io;
    unsigned int gsi;
    unsigned int entry;
    unsigned int low;

    if (!apic_available() || irq >= ACPI_ISA_IRQ_COUNT ||
        (enabled_irq_bitmap & (1U << irq)) == 0U) {
        return 0;
    }
    override = &firmware_madt->isa_overrides[irq];
    gsi = override->present != 0U ? override->global_interrupt : irq;
    io = io_apic_for_gsi(gsi, &entry);
    if (io == 0) {
        return 0;
    }
    low = io_read(io, redirection_low_register(entry));
    return (low & IO_APIC_MASKED) == 0U &&
        (low & 0xFFU) == LEGACY_IRQ_BASE + irq;
}

void apic_acknowledge(void)
{
    if (apic_available()) {
        local_write(LOCAL_APIC_EOI, 0U);
    }
}

unsigned int apic_local_id(void)
{
    return apic_available() ? current_local_apic_id : 0U;
}
