#include "fat.h"

#define FAT_SECTOR_SIZE 512U
#define FAT16_EOC 0xFFF8U
#define FAT16_BAD 0xFFF7U
#define FAT_ATTR_DIRECTORY 0x10U
#define FAT_ATTR_LFN 0x0FU

static unsigned int last_error;

static int read_sector(const struct fat_volume *volume, fat_u32_t lba, fat_u8_t *buffer)
{
    if (volume == 0 || !volume->mounted || lba < volume->start_lba ||
        lba - volume->start_lba >= volume->total_sectors) {
        return 0;
    }
    return volume->read(lba, 1, buffer);
}

static fat_u16_t read_u16(const fat_u8_t *data, unsigned int offset)
{
    return (fat_u16_t)data[offset] | ((fat_u16_t)data[offset + 1U] << 8U);
}

static fat_u32_t read_u32(const fat_u8_t *data, unsigned int offset)
{
    return (fat_u32_t)data[offset] | ((fat_u32_t)data[offset + 1U] << 8U) |
        ((fat_u32_t)data[offset + 2U] << 16U) | ((fat_u32_t)data[offset + 3U] << 24U);
}

static unsigned int string_length(const char *value, unsigned int limit)
{
    unsigned int length = 0;

    if (value == 0) {
        return limit;
    }
    while (length < limit && value[length] != '\0') {
        length++;
    }
    return length;
}

static int short_name_from_text(const char *text, fat_u8_t *short_name)
{
    unsigned int index = 0;
    unsigned int output = 0;
    unsigned int length = string_length(text, 13U);
    unsigned int dot = length;

    if (length == 0U || length >= 13U) {
        return 0;
    }
    while (index < length) {
        if (text[index] == '.') {
            if (dot != length || index == 0U || index + 1U == length) {
                return 0;
            }
            dot = index;
        }
        index++;
    }
    if (dot > 8U || length - dot - 1U > 3U) {
        return 0;
    }
    for (index = 0; index < 11U; index++) {
        short_name[index] = ' ';
    }
    for (index = 0; index < length; index++) {
        fat_u8_t character = (fat_u8_t)text[index];
        if (character == '.') {
            output = 8U;
            continue;
        }
        if (character < '!' || character == '/' || character == '\\' ||
            character == ':' || character == '"' || character == '*' ||
            character == '?' || character == '<' || character == '>' || character == '|') {
            return 0;
        }
        if (character >= 'a' && character <= 'z') {
            character = (fat_u8_t)(character - ('a' - 'A'));
        }
        if (output >= 11U) {
            return 0;
        }
        short_name[output++] = character;
    }
    return output != 0U;
}

static int name_matches(const fat_u8_t *entry, const fat_u8_t *short_name)
{
    unsigned int index;

    for (index = 0; index < 11U; index++) {
        if (entry[index] != short_name[index]) {
            return 0;
        }
    }
    return 1;
}

static void entry_to_text(const fat_u8_t *entry, char *name, unsigned int capacity)
{
    unsigned int index;
    unsigned int output = 0;

    if (capacity == 0U) {
        return;
    }
    for (index = 0; index < 8U && entry[index] != ' '; index++) {
        if (output + 1U < capacity) {
            name[output++] = (char)entry[index];
        }
    }
    if (entry[8] != ' ' && output + 1U < capacity) {
        name[output++] = '.';
        for (index = 8U; index < 11U && entry[index] != ' '; index++) {
            if (output + 1U < capacity) {
                name[output++] = (char)entry[index];
            }
        }
    }
    name[output] = '\0';
}

static int find_entry(const struct fat_volume *volume, const fat_u8_t *short_name,
    fat_u8_t *entry, unsigned int *entry_index)
{
    fat_u8_t sector[FAT_SECTOR_SIZE];
    fat_u32_t sector_index;
    unsigned int item;

    for (sector_index = 0; sector_index < (volume->root_entries + 15U) / 16U; sector_index++) {
        if (!read_sector(volume, volume->root_start + sector_index, sector)) {
            return 0;
        }
        for (item = 0; item < 16U; item++) {
            fat_u8_t *candidate = sector + (item * 32U);
            if (candidate[0] == 0U) {
                return 0;
            }
            if (candidate[0] == 0xE5U || candidate[11] == FAT_ATTR_LFN ||
                (candidate[11] & FAT_ATTR_DIRECTORY) != 0U) {
                continue;
            }
            if (name_matches(candidate, short_name)) {
                unsigned int index;
                for (index = 0; index < 32U; index++) {
                    entry[index] = candidate[index];
                }
                if (entry_index != 0) {
                    *entry_index = (sector_index * 16U) + item;
                }
                return 1;
            }
        }
    }
    return 0;
}

int fat_mount(struct fat_volume *volume, fat_read_fn read, fat_u32_t start_lba)
{
    fat_u8_t boot[FAT_SECTOR_SIZE];
    fat_u32_t total_sectors;
    fat_u32_t root_sectors;
    fat_u32_t data_sectors;
    fat_u16_t bytes_per_sector;
    fat_u8_t sectors_per_cluster;
    fat_u16_t reserved;
    fat_u8_t fats;
    fat_u16_t root_entries;
    fat_u16_t sectors_per_fat;

    last_error = 0;
    if (volume == 0 || read == 0) {
        last_error = 1;
        return 0;
    }
    volume->mounted = 0;
    volume->read = read;
    volume->start_lba = start_lba;
    if (!read(start_lba, 1, boot)) {
        last_error = 2;
        return 0;
    }
    if (boot[510] != 0x55U || boot[511] != 0xAAU) {
        last_error = 3;
        return 0;
    }
    bytes_per_sector = read_u16(boot, 11);
    sectors_per_cluster = boot[13];
    reserved = read_u16(boot, 14);
    fats = boot[16];
    root_entries = read_u16(boot, 17);
    sectors_per_fat = read_u16(boot, 22);
    total_sectors = read_u16(boot, 19);
    if (total_sectors == 0U) {
        total_sectors = read_u32(boot, 32);
    }
    if (bytes_per_sector != FAT_SECTOR_SIZE || sectors_per_cluster == 0U ||
        sectors_per_cluster > 128U ||
        (sectors_per_cluster & (sectors_per_cluster - 1U)) != 0U || reserved == 0U ||
        fats == 0U || root_entries == 0U || sectors_per_fat == 0U || total_sectors == 0U) {
        last_error = 4;
        return 0;
    }
    root_sectors = ((fat_u32_t)root_entries * 32U + FAT_SECTOR_SIZE - 1U) / FAT_SECTOR_SIZE;
    if (total_sectors <= (fat_u32_t)reserved + ((fat_u32_t)fats * sectors_per_fat) + root_sectors) {
        last_error = 5;
        return 0;
    }
    data_sectors = total_sectors - reserved - ((fat_u32_t)fats * sectors_per_fat) - root_sectors;
    if (data_sectors == 0U || data_sectors / sectors_per_cluster < 4085U ||
        data_sectors / sectors_per_cluster >= 65525U) {
        last_error = 6;
        return 0;
    }
    if (start_lba > 0xFFFFFFFFU - total_sectors) {
        last_error = 7;
        return 0;
    }
    volume->total_sectors = total_sectors;
    volume->fat_start = start_lba + reserved;
    volume->root_start = volume->fat_start + ((fat_u32_t)fats * sectors_per_fat);
    volume->data_start = volume->root_start + root_sectors;
    volume->sectors_per_fat = sectors_per_fat;
    volume->root_entries = root_entries;
    volume->sectors_per_cluster = sectors_per_cluster;
    volume->cluster_count = data_sectors / sectors_per_cluster;
    volume->mounted = 1;
    return 1;
}

unsigned int fat_last_error(void)
{
    return last_error;
}

unsigned int fat_file_count(const struct fat_volume *volume)
{
    fat_u8_t sector[FAT_SECTOR_SIZE];
    fat_u32_t sector_index;
    unsigned int item;
    unsigned int count = 0;

    if (volume == 0 || !volume->mounted) {
        return 0;
    }
    for (sector_index = 0; sector_index < (volume->root_entries + 15U) / 16U; sector_index++) {
        if (!read_sector(volume, volume->root_start + sector_index, sector)) {
            return count;
        }
        for (item = 0; item < 16U; item++) {
            const fat_u8_t *entry = sector + (item * 32U);
            if (entry[0] == 0U) {
                return count;
            }
            if (entry[0] != 0xE5U && entry[11] != FAT_ATTR_LFN &&
                (entry[11] & FAT_ATTR_DIRECTORY) == 0U) {
                count++;
            }
        }
    }
    return count;
}

int fat_file_name(const struct fat_volume *volume, unsigned int index, char *name, unsigned int capacity)
{
    fat_u8_t sector[FAT_SECTOR_SIZE];
    fat_u32_t sector_index;
    unsigned int item;
    unsigned int count = 0;

    if (volume == 0 || !volume->mounted || name == 0 || capacity == 0U) {
        return 0;
    }
    for (sector_index = 0; sector_index < (volume->root_entries + 15U) / 16U; sector_index++) {
        if (!read_sector(volume, volume->root_start + sector_index, sector)) {
            return 0;
        }
        for (item = 0; item < 16U; item++) {
            fat_u8_t *entry = sector + (item * 32U);
            if (entry[0] == 0U) {
                return 0;
            }
            if (entry[0] == 0xE5U || entry[11] == FAT_ATTR_LFN ||
                (entry[11] & FAT_ATTR_DIRECTORY) != 0U) {
                continue;
            }
            if (count++ == index) {
                entry_to_text(entry, name, capacity);
                return 1;
            }
        }
    }
    return 0;
}

int fat_read_file(const struct fat_volume *volume, const char *name, void *buffer,
    unsigned int capacity, unsigned int *size)
{
    fat_u8_t short_name[11];
    fat_u8_t entry[32];
    fat_u8_t sector[FAT_SECTOR_SIZE];
    fat_u8_t fat_sector[FAT_SECTOR_SIZE];
    fat_u32_t remaining;
    fat_u32_t cluster;
    fat_u32_t output = 0;
    fat_u32_t guard = 0;
    fat_u32_t max_sectors;

    if (size != 0) {
        *size = 0;
    }
    if (volume == 0 || !volume->mounted || buffer == 0 || !short_name_from_text(name, short_name) ||
        !find_entry(volume, short_name, entry, 0)) {
        return 0;
    }
    remaining = read_u32(entry, 28);
    max_sectors = volume->cluster_count * volume->sectors_per_cluster;
    if (remaining > capacity || remaining / FAT_SECTOR_SIZE > max_sectors ||
        ((remaining % FAT_SECTOR_SIZE) != 0U && remaining / FAT_SECTOR_SIZE >= max_sectors)) {
        return 0;
    }
    cluster = read_u16(entry, 26);
    while (remaining != 0U) {
        fat_u32_t sector_index;
        fat_u32_t cluster_offset;

        if (cluster < 2U || cluster >= volume->cluster_count + 2U || guard++ > volume->cluster_count) {
            return 0;
        }
        for (sector_index = 0; sector_index < volume->sectors_per_cluster && remaining != 0U; sector_index++) {
            fat_u32_t amount;
            if (!read_sector(volume, volume->data_start + ((cluster - 2U) * volume->sectors_per_cluster) + sector_index, sector)) {
                return 0;
            }
            amount = remaining < FAT_SECTOR_SIZE ? remaining : FAT_SECTOR_SIZE;
            for (cluster_offset = 0; cluster_offset < amount; cluster_offset++) {
                ((fat_u8_t *)buffer)[output + cluster_offset] = sector[cluster_offset];
            }
            output += amount;
            remaining -= amount;
        }
        if (remaining == 0U) {
            break;
        }
        cluster_offset = cluster * 2U;
        if (!read_sector(volume, volume->fat_start + cluster_offset / FAT_SECTOR_SIZE, fat_sector)) {
            return 0;
        }
        cluster = read_u16(fat_sector, cluster_offset % FAT_SECTOR_SIZE);
        if (cluster == FAT16_BAD || cluster >= FAT16_EOC) {
            return 0;
        }
    }
    if (size != 0) {
        *size = output;
    }
    return 1;
}
