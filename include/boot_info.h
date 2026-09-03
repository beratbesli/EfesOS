#ifndef EFESOS_BOOT_INFO_H
#define EFESOS_BOOT_INFO_H

typedef unsigned char boot_u8_t;
typedef unsigned int boot_u32_t;

#define BOOT_INFO_MAGIC 0x534F4645U
#define BOOT_INFO_MAX_MEMORY_MAP_ENTRIES 32U
#define BOOT_MEMORY_AVAILABLE 1U
#define BOOT_VIDEO_FONT_AVAILABLE 1U
#define BOOT_KERNEL_INTEGRITY_VERIFIED 2U
#define BOOT_INFO_KNOWN_FLAGS (BOOT_VIDEO_FONT_AVAILABLE | BOOT_KERNEL_INTEGRITY_VERIFIED)
#define BOOT_MEMORY_KNOWN_ATTRIBUTES 3U
#define BOOT_INFO_ACPI_RSDP_OFFSET 792U
/* Compatibility name for older callers; stage-2 now uses SHA-256. */
#define BOOT_KERNEL_CHECKSUM_VERIFIED BOOT_KERNEL_INTEGRITY_VERIFIED

struct boot_memory_map_entry {
    boot_u32_t base_low;
    boot_u32_t base_high;
    boot_u32_t length_low;
    boot_u32_t length_high;
    boot_u32_t type;
    boot_u32_t attributes;
} __attribute__((packed));

struct boot_info {
    boot_u32_t magic;
    boot_u32_t boot_drive;
    boot_u32_t memory_map_entry_count;
    boot_u32_t memory_map_entry_size;
    boot_u32_t vga_font_address;
    boot_u32_t video_flags;
    struct boot_memory_map_entry memory_map[BOOT_INFO_MAX_MEMORY_MAP_ENTRIES];
    boot_u32_t acpi_rsdp_address;
} __attribute__((packed));

_Static_assert(__builtin_offsetof(struct boot_info, acpi_rsdp_address) ==
    BOOT_INFO_ACPI_RSDP_OFFSET, "boot_info ACPI ABI offset changed");
_Static_assert(sizeof(struct boot_info) == BOOT_INFO_ACPI_RSDP_OFFSET + 4U,
    "boot_info ABI size changed");

static inline int boot_info_is_valid(const struct boot_info *info)
{
    boot_u32_t index;

    if (info == 0 || info->magic != BOOT_INFO_MAGIC ||
        info->boot_drive > 0xFFU ||
        info->memory_map_entry_size != sizeof(struct boot_memory_map_entry) ||
        info->memory_map_entry_count == 0 ||
        info->memory_map_entry_count > BOOT_INFO_MAX_MEMORY_MAP_ENTRIES ||
        (info->video_flags & ~BOOT_INFO_KNOWN_FLAGS) != 0U ||
        ((info->video_flags & BOOT_VIDEO_FONT_AVAILABLE) != 0U &&
         (info->vga_font_address < 0x1000U || info->vga_font_address >= 0x003FF000U)) ||
        ((info->video_flags & BOOT_VIDEO_FONT_AVAILABLE) == 0U &&
         info->vga_font_address != 0U) ||
        (info->acpi_rsdp_address != 0U &&
         (((info->acpi_rsdp_address & 0x0FU) != 0U) ||
          !((info->acpi_rsdp_address >= 0x00080000U &&
             info->acpi_rsdp_address < 0x000A0000U) ||
            (info->acpi_rsdp_address >= 0x000E0000U &&
             info->acpi_rsdp_address < 0x00100000U))))) {
        return 0;
    }
    for (index = 0; index < info->memory_map_entry_count; index++) {
        const struct boot_memory_map_entry *entry = &info->memory_map[index];
        unsigned long long base = ((unsigned long long)entry->base_high << 32U) |
            entry->base_low;
        unsigned long long length = ((unsigned long long)entry->length_high << 32U) |
            entry->length_low;
        unsigned long long end = base + length;

        if (length == 0U || end < base || entry->type == 0U ||
            (entry->attributes & ~BOOT_MEMORY_KNOWN_ATTRIBUTES) != 0U) {
            return 0;
        }
    }
    return 1;
}

#endif
