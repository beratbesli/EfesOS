#ifndef EFESOS_BOOT_INFO_H
#define EFESOS_BOOT_INFO_H

typedef unsigned char boot_u8_t;
typedef unsigned int boot_u32_t;

#define BOOT_INFO_MAGIC 0x534F4645U
#define BOOT_INFO_MAX_MEMORY_MAP_ENTRIES 32U
#define BOOT_MEMORY_AVAILABLE 1U

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
    struct boot_memory_map_entry memory_map[BOOT_INFO_MAX_MEMORY_MAP_ENTRIES];
} __attribute__((packed));

static inline int boot_info_is_valid(const struct boot_info *info)
{
    return info != 0 &&
           info->magic == BOOT_INFO_MAGIC &&
           info->memory_map_entry_size == sizeof(struct boot_memory_map_entry) &&
           info->memory_map_entry_count != 0 &&
           info->memory_map_entry_count <= BOOT_INFO_MAX_MEMORY_MAP_ENTRIES;
}

#endif
