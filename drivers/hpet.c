#include "hpet.h"
#include "hpet_time.h"
#include "paging.h"

#define HPET_MAP_VIRTUAL 0xCEF00000U
#define HPET_CAPABILITIES_OFFSET 0x000U
#define HPET_CONFIGURATION_OFFSET 0x010U
#define HPET_MAIN_COUNTER_OFFSET 0x0F0U
#define HPET_TIMER_CONFIGURATION_OFFSET 0x100U
#define HPET_TIMER_REGISTER_STRIDE 0x020U
#define HPET_CONFIGURATION_ENABLE 0x001U
#define HPET_CONFIGURATION_LEGACY 0x002U
#define HPET_TIMER_INTERRUPT_ENABLE 0x004U
#define HPET_CAP_COUNTER_64BIT (1U << 13U)
#define HPET_MAX_PERIOD_FEMTOSECONDS 100000000U
#define HPET_SELF_TEST_READ_LIMIT 1000000U

static volatile unsigned char *hpet_registers;
static unsigned int hpet_physical_page;
static unsigned int hpet_page_count;
static unsigned int hpet_period;
static unsigned int hpet_counter_high;
static unsigned int hpet_counter_last_low;
static int hpet_has_64bit_counter;
static int initialized;
static int available;

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

static unsigned int read_register32(unsigned int offset)
{
    volatile unsigned int *reg =
        (volatile unsigned int *)(hpet_registers + offset);
    unsigned int value = *reg;

    __asm__ volatile ("" : : : "memory");
    return value;
}

static void write_register32(unsigned int offset, unsigned int value)
{
    volatile unsigned int *reg =
        (volatile unsigned int *)(hpet_registers + offset);

    __asm__ volatile ("" : : : "memory");
    *reg = value;
    __asm__ volatile ("" : : : "memory");
}

static hpet_tick_t read_counter_64bit(void)
{
    unsigned int high_before;
    unsigned int high_after;
    unsigned int low;

    do {
        high_before = read_register32(HPET_MAIN_COUNTER_OFFSET + 4U);
        low = read_register32(HPET_MAIN_COUNTER_OFFSET);
        high_after = read_register32(HPET_MAIN_COUNTER_OFFSET + 4U);
    } while (high_before != high_after);
    return ((hpet_tick_t)high_after << 32U) | low;
}

static int release_mapping(void)
{
    unsigned int index;
    int result = 1;

    if (hpet_registers == 0) {
        return 1;
    }
    for (index = 0U; index < hpet_page_count; index++) {
        unsigned int physical = paging_unmap_page(
            HPET_MAP_VIRTUAL + index * PAGE_SIZE);

        if (physical != hpet_physical_page + index * PAGE_SIZE) {
            result = 0;
        }
    }
    hpet_registers = 0;
    hpet_page_count = 0U;
    return result;
}

int hpet_init(const struct acpi_hpet_info *firmware_info)
{
    unsigned int physical_page;
    unsigned int page_offset;
    unsigned int capability_low;
    unsigned int capability_high;
    unsigned int configuration;
    unsigned int timer_count;
    unsigned int required_bytes;
    unsigned int timer;

    if (initialized) {
        return available;
    }
    initialized = 1;
    available = 0;
    hpet_registers = 0;
    hpet_page_count = 0U;
    hpet_period = 0U;
    hpet_counter_high = 0U;
    hpet_counter_last_low = 0U;
    hpet_has_64bit_counter = 0;
    if (firmware_info == 0 ||
        paging_current_directory() != paging_kernel_directory() ||
        firmware_info->physical_address < PAGE_SIZE ||
        (firmware_info->physical_address & 0x3FFU) != 0U ||
        firmware_info->physical_address > 0xFFFFFFFFU - 1023U) {
        return 0;
    }
    physical_page = firmware_info->physical_address & ~(PAGE_SIZE - 1U);
    page_offset = firmware_info->physical_address & (PAGE_SIZE - 1U);
    if (paging_is_mapped(HPET_MAP_VIRTUAL) ||
        !paging_map_page(HPET_MAP_VIRTUAL, physical_page,
            PAGE_FLAG_WRITABLE | PAGE_FLAG_CACHE_DISABLE)) {
        return 0;
    }
    hpet_physical_page = physical_page;
    hpet_page_count = 1U;
    hpet_registers = (volatile unsigned char *)(HPET_MAP_VIRTUAL + page_offset);
    capability_low = read_register32(HPET_CAPABILITIES_OFFSET);
    capability_high = read_register32(HPET_CAPABILITIES_OFFSET + 4U);
    if (capability_low != firmware_info->event_timer_block_id ||
        capability_high == 0U ||
        capability_high > HPET_MAX_PERIOD_FEMTOSECONDS) {
        release_mapping();
        return 0;
    }
    timer_count = ((capability_low >> 8U) & 0x1FU) + 1U;
    required_bytes = HPET_TIMER_CONFIGURATION_OFFSET +
        (timer_count - 1U) * HPET_TIMER_REGISTER_STRIDE + 4U;
    if (page_offset + required_bytes > PAGE_SIZE) {
        if (physical_page > 0xFFFFFFFFU - PAGE_SIZE ||
            paging_is_mapped(HPET_MAP_VIRTUAL + PAGE_SIZE) ||
            !paging_map_page(HPET_MAP_VIRTUAL + PAGE_SIZE,
                physical_page + PAGE_SIZE,
                PAGE_FLAG_WRITABLE | PAGE_FLAG_CACHE_DISABLE)) {
            release_mapping();
            return 0;
        }
        hpet_page_count = 2U;
    }
    configuration = read_register32(HPET_CONFIGURATION_OFFSET);
    write_register32(HPET_CONFIGURATION_OFFSET,
        configuration & ~HPET_CONFIGURATION_ENABLE);
    for (timer = 0U; timer < timer_count; timer++) {
        unsigned int offset = HPET_TIMER_CONFIGURATION_OFFSET +
            timer * HPET_TIMER_REGISTER_STRIDE;
        unsigned int timer_configuration = read_register32(offset);

        write_register32(offset,
            timer_configuration & ~HPET_TIMER_INTERRUPT_ENABLE);
        if ((read_register32(offset) & HPET_TIMER_INTERRUPT_ENABLE) != 0U) {
            write_register32(HPET_CONFIGURATION_OFFSET, configuration);
            release_mapping();
            return 0;
        }
    }
    write_register32(HPET_MAIN_COUNTER_OFFSET + 4U, 0U);
    write_register32(HPET_MAIN_COUNTER_OFFSET, 0U);
    write_register32(HPET_CONFIGURATION_OFFSET,
        (configuration | HPET_CONFIGURATION_ENABLE) &
        ~HPET_CONFIGURATION_LEGACY);
    if ((read_register32(HPET_CONFIGURATION_OFFSET) &
            (HPET_CONFIGURATION_ENABLE | HPET_CONFIGURATION_LEGACY)) !=
            HPET_CONFIGURATION_ENABLE) {
        write_register32(HPET_CONFIGURATION_OFFSET, configuration);
        release_mapping();
        return 0;
    }
    hpet_period = capability_high;
    hpet_has_64bit_counter =
        (capability_low & HPET_CAP_COUNTER_64BIT) != 0U;
    hpet_counter_last_low = read_register32(HPET_MAIN_COUNTER_OFFSET);
    available = 1;
    return 1;
}

int hpet_available(void)
{
    return initialized && available;
}

unsigned int hpet_period_femtoseconds(void)
{
    return hpet_available() ? hpet_period : 0U;
}

int hpet_counter_is_64bit(void)
{
    return hpet_available() && hpet_has_64bit_counter;
}

hpet_tick_t hpet_ticks(void)
{
    unsigned int flags;
    unsigned int low;
    hpet_tick_t ticks;

    if (!hpet_available()) {
        return 0U;
    }
    if (hpet_has_64bit_counter) {
        return read_counter_64bit();
    }
    flags = interrupt_save();
    low = read_register32(HPET_MAIN_COUNTER_OFFSET);
    ticks = hpet_extend_counter32(low, &hpet_counter_last_low,
        &hpet_counter_high);
    interrupt_restore(flags);
    return ticks;
}

hpet_tick_t hpet_nanoseconds(void)
{
    if (!hpet_available()) {
        return 0U;
    }
    return hpet_ticks_to_nanoseconds(hpet_ticks(), hpet_period);
}

void hpet_maintain(void)
{
    if (hpet_available() && !hpet_has_64bit_counter) {
        (void)hpet_ticks();
    }
}

int hpet_self_test(void)
{
    hpet_tick_t before;
    hpet_tick_t after;
    unsigned int attempt;

    if (!hpet_available()) {
        return 0;
    }
    before = hpet_ticks();
    for (attempt = 0U; attempt < HPET_SELF_TEST_READ_LIMIT; attempt++) {
        after = hpet_ticks();
        if (after > before) {
            return hpet_nanoseconds() >=
                hpet_ticks_to_nanoseconds(after, hpet_period);
        }
        __asm__ volatile ("pause");
    }
    return 0;
}
