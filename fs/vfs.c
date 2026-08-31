#include "ata.h"
#include "fat.h"
#include "vfs.h"

static struct fat_volume volume;

static fat_u32_t read_u32(const fat_u8_t *data, unsigned int offset)
{
    return (fat_u32_t)data[offset] | ((fat_u32_t)data[offset + 1U] << 8U) |
        ((fat_u32_t)data[offset + 2U] << 16U) | ((fat_u32_t)data[offset + 3U] << 24U);
}

void vfs_init(void)
{
    volume.mounted = 0;
    if (ata_present()) {
        if (fat_mount(&volume, ata_read_sectors, 0)) {
            return;
        }
        {
            fat_u8_t mbr[512];
            unsigned int index;

            if (!ata_read_sectors(0, 1, mbr) || mbr[510] != 0x55U || mbr[511] != 0xAAU) {
                return;
            }
            for (index = 0; index < 4U; index++) {
                const fat_u8_t *partition = mbr + 446U + (index * 16U);
                fat_u8_t type = partition[4];
                fat_u32_t start = read_u32(partition, 8);
                fat_u32_t length = read_u32(partition, 12);

                if ((type == 0x04U || type == 0x06U || type == 0x0EU) && length != 0U &&
                    start < ata_sector_count() && length <= ata_sector_count() - start &&
                    fat_mount(&volume, ata_read_sectors, start)) {
                    return;
                }
            }
        }
    }
}

int vfs_is_mounted(void)
{
    return volume.mounted;
}

unsigned int vfs_file_count(void)
{
    return fat_file_count(&volume);
}

int vfs_file_name(unsigned int index, char *name, unsigned int capacity)
{
    return fat_file_name(&volume, index, name, capacity);
}

int vfs_read_file(const char *name, void *buffer, unsigned int capacity, unsigned int *size)
{
    return fat_read_file(&volume, name, buffer, capacity, size);
}
