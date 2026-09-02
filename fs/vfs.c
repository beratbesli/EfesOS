#include "block_device.h"
#include "fat.h"
#include "vfs.h"

static struct fat_volume volume;
static const struct block_device *storage_device;

#define ATA_READ_RETRIES 3U

static int read_with_retry(fat_u32_t lba, fat_u8_t count, void *buffer)
{
    unsigned int attempt;

    for (attempt = 0; attempt < ATA_READ_RETRIES; attempt++) {
        if (block_device_read(storage_device, lba, count, buffer)) {
            return 1;
        }
    }
    return 0;
}

static int mounted_volume_fits_device(void)
{
    unsigned int device_sectors = block_device_sector_count(storage_device);

    return volume.mounted && volume.start_lba < device_sectors &&
        volume.total_sectors != 0U && volume.total_sectors <=
        device_sectors - volume.start_lba;
}

static fat_u32_t read_u32(const fat_u8_t *data, unsigned int offset)
{
    return (fat_u32_t)data[offset] | ((fat_u32_t)data[offset + 1U] << 8U) |
        ((fat_u32_t)data[offset + 2U] << 16U) | ((fat_u32_t)data[offset + 3U] << 24U);
}

void vfs_init(const struct block_device *device)
{
    volume.mounted = 0;
    storage_device = 0;
    if (block_device_is_ready(device)) {
        unsigned int device_sectors = block_device_sector_count(device);

        storage_device = device;
        if (fat_mount(&volume, read_with_retry, 0) && mounted_volume_fits_device()) {
            return;
        }
        volume.mounted = 0;
        {
            fat_u8_t mbr[512];
            unsigned int index;

            if (!read_with_retry(0, 1, mbr) || mbr[510] != 0x55U || mbr[511] != 0xAAU) {
                return;
            }
            for (index = 0; index < 4U; index++) {
                const fat_u8_t *partition = mbr + 446U + (index * 16U);
                fat_u8_t type = partition[4];
                fat_u32_t start = read_u32(partition, 8);
                fat_u32_t length = read_u32(partition, 12);

                if ((type == 0x04U || type == 0x06U || type == 0x0EU) && length != 0U &&
                    start < device_sectors && length <= device_sectors - start &&
                    fat_mount(&volume, read_with_retry, start) && mounted_volume_fits_device()) {
                    return;
                }
                volume.mounted = 0;
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

int vfs_journal_region_available(unsigned int start_lba, unsigned int sector_count)
{
    unsigned int device_sectors = block_device_sector_count(storage_device);
    unsigned int region_end;

    if (sector_count == 0U || device_sectors == 0U || start_lba >= device_sectors ||
        (sector_count - 1U) > 0xFFFFFFFFU - start_lba ||
        sector_count > device_sectors - start_lba) {
        return 0;
    }
    region_end = start_lba + sector_count;
    if (!volume.mounted) {
        return 1;
    }
    /* fat_mount/vfs_init already proved this addition fits the device. */
    if (start_lba >= volume.start_lba + volume.total_sectors ||
        region_end <= volume.start_lba) {
        return 1;
    }
    return 0;
}
