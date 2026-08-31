#include "ata.h"
#include "fat.h"
#include "vfs.h"

static struct fat_volume volume;

void vfs_init(void)
{
    volume.mounted = 0;
    if (ata_present()) {
        fat_mount(&volume, ata_read_sectors, 0);
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
