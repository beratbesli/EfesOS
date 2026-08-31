#include <stdio.h>

#include "boot_info.h"

static void make_valid(struct boot_info *info)
{
    unsigned int index;

    for (index = 0U; index < sizeof(*info); index++) {
        ((unsigned char *)info)[index] = 0U;
    }
    info->magic = BOOT_INFO_MAGIC;
    info->memory_map_entry_count = 1U;
    info->memory_map_entry_size = sizeof(struct boot_memory_map_entry);
    info->memory_map[0].base_low = 0x00100000U;
    info->memory_map[0].length_low = 0x00100000U;
    info->memory_map[0].type = BOOT_MEMORY_AVAILABLE;
    info->memory_map[0].attributes = 1U;
    info->video_flags = BOOT_VIDEO_FONT_AVAILABLE;
    info->vga_font_address = 0x000F0000U;
}

int main(void)
{
    struct boot_info info;

    make_valid(&info);
    if (!boot_info_is_valid(&info)) {
        return 1;
    }
    info.memory_map[0].length_low = 0U;
    if (boot_info_is_valid(&info)) {
        return 1;
    }
    make_valid(&info);
    info.memory_map[0].base_low = 0xFFFFFFFFU;
    info.memory_map[0].base_high = 0xFFFFFFFFU;
    info.memory_map[0].length_low = 1U;
    if (boot_info_is_valid(&info)) {
        return 1;
    }
    make_valid(&info);
    info.vga_font_address = 0x00400000U;
    if (boot_info_is_valid(&info)) {
        return 1;
    }
    puts("Boot metadata host self-test passed.");
    return 0;
}
