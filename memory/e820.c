#include "e820.h"

static e820_u64_t entry_base(const struct boot_memory_map_entry *entry)
{
    return ((e820_u64_t)entry->base_high << 32U) | entry->base_low;
}

static e820_u64_t entry_length(const struct boot_memory_map_entry *entry)
{
    return ((e820_u64_t)entry->length_high << 32U) | entry->length_low;
}

int e820_apply_memory_map(const struct boot_info *info,
    e820_range_handler_t usable_handler,
    e820_range_handler_t reserved_handler,
    void *context)
{
    boot_u32_t index;

    if (!boot_info_is_valid(info) || usable_handler == 0 || reserved_handler == 0) {
        return 0;
    }

    for (index = 0U; index < info->memory_map_entry_count; index++) {
        const struct boot_memory_map_entry *entry = &info->memory_map[index];

        if ((entry->attributes & 1U) != 0U && entry->type == BOOT_MEMORY_AVAILABLE) {
            usable_handler(entry_base(entry), entry_length(entry), context);
        }
    }
    for (index = 0U; index < info->memory_map_entry_count; index++) {
        const struct boot_memory_map_entry *entry = &info->memory_map[index];

        if ((entry->attributes & 1U) != 0U && entry->type != BOOT_MEMORY_AVAILABLE) {
            reserved_handler(entry_base(entry), entry_length(entry), context);
        }
    }
    return 1;
}
