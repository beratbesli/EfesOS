#include <stdio.h>
#include <string.h>

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

static unsigned int next_random(unsigned int *state)
{
    *state = (*state * 1664525U) + 1013904223U;
    return *state;
}

static int validator_is_stable_and_read_only(struct boot_info *info)
{
    struct boot_info before;
    int first;
    int second;

    memcpy(&before, info, sizeof(before));
    first = boot_info_is_valid(info);
    second = boot_info_is_valid(info);
    return first == second && memcmp(&before, info, sizeof(before)) == 0;
}

static int mutation_scan(void)
{
    struct boot_info info;
    struct boot_info baseline;
    unsigned char *bytes = (unsigned char *)&info;
    unsigned int offset;
    unsigned int value;
    unsigned int iteration;
    unsigned int state = 0xE8205A5AU;

    make_valid(&baseline);
    if (!boot_info_is_valid(&baseline) || boot_info_is_valid(0)) {
        return 0;
    }

    /* Exercise every single-byte value at every offset. The validator must
       be deterministic and must never alter bootloader-owned metadata. */
    for (offset = 0U; offset < sizeof(info); offset++) {
        for (value = 0U; value < 256U; value++) {
            memcpy(&info, &baseline, sizeof(info));
            bytes[offset] = (unsigned char)value;
            if (!validator_is_stable_and_read_only(&info)) {
                return 0;
            }
        }
    }

    /* Multi-byte corruption reaches combinations that a single-byte scan
       cannot, while the fixed seed keeps failures exactly reproducible. */
    for (iteration = 0U; iteration < 16384U; iteration++) {
        unsigned int mutation;
        unsigned int mutation_count = 1U + (next_random(&state) % 32U);

        memcpy(&info, &baseline, sizeof(info));
        for (mutation = 0U; mutation < mutation_count; mutation++) {
            offset = next_random(&state) % sizeof(info);
            bytes[offset] ^= (unsigned char)(next_random(&state) >> 24U);
        }
        if (!validator_is_stable_and_read_only(&info)) {
            return 0;
        }
    }

    /* Inactive E820 slots are outside the declared input and must not affect
       the decision, even if firmware left arbitrary bytes in those slots. */
    memcpy(&info, &baseline, sizeof(info));
    for (offset = 0U; offset < sizeof(info.memory_map[1]); offset++) {
        ((unsigned char *)&info.memory_map[1])[offset] = (unsigned char)(offset ^ 0xA5U);
    }
    return boot_info_is_valid(&info);
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
    make_valid(&info);
    info.boot_drive = 0x100U;
    if (boot_info_is_valid(&info)) {
        return 1;
    }
    make_valid(&info);
    info.video_flags |= 0x80000000U;
    if (boot_info_is_valid(&info)) {
        return 1;
    }
    make_valid(&info);
    info.video_flags = BOOT_KERNEL_INTEGRITY_VERIFIED;
    if (boot_info_is_valid(&info)) {
        return 1;
    }
    make_valid(&info);
    info.memory_map[0].type = 0U;
    if (boot_info_is_valid(&info)) {
        return 1;
    }
    make_valid(&info);
    info.memory_map[0].attributes |= 4U;
    if (boot_info_is_valid(&info)) {
        return 1;
    }
    make_valid(&info);
    info.acpi_rsdp_address = 0x000E0010U;
    if (!boot_info_is_valid(&info)) {
        return 1;
    }
    info.acpi_rsdp_address++;
    if (boot_info_is_valid(&info) || !mutation_scan()) {
        return 1;
    }
    puts("Boot metadata property-fuzz self-test passed (219136 deterministic mutations).");
    return 0;
}
