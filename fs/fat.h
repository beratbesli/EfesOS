#ifndef EFESOS_FAT_H
#define EFESOS_FAT_H

typedef unsigned char fat_u8_t;
typedef unsigned short fat_u16_t;
typedef unsigned int fat_u32_t;
typedef int (*fat_read_fn)(fat_u32_t lba, fat_u8_t count, void *buffer);

struct fat_volume {
    fat_read_fn read;
    fat_u32_t start_lba;
    fat_u32_t total_sectors;
    fat_u32_t fat_start;
    fat_u32_t root_start;
    fat_u32_t data_start;
    fat_u32_t sectors_per_fat;
    fat_u32_t root_entries;
    fat_u32_t sectors_per_cluster;
    fat_u32_t cluster_count;
    int mounted;
};

int fat_mount(struct fat_volume *volume, fat_read_fn read, fat_u32_t start_lba);
unsigned int fat_last_error(void);
unsigned int fat_file_count(const struct fat_volume *volume);
int fat_file_name(const struct fat_volume *volume, unsigned int index, char *name, unsigned int capacity);
int fat_read_file(const struct fat_volume *volume, const char *name, void *buffer,
    unsigned int capacity, unsigned int *size);

#endif
