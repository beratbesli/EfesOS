#include "block_device.h"

#define BLOCK_DEVICE_MAGIC 0x424C4B44U

void block_device_reset(struct block_device *device)
{
    if (device == 0) {
        return;
    }
    device->magic = 0U;
    device->sector_count = 0U;
    device->sector_size = 0U;
    device->max_transfer_sectors = 0U;
    device->read = 0;
    device->write = 0;
    device->context = 0;
}

int block_device_configure(struct block_device *device, unsigned int sector_count,
    unsigned short sector_size, unsigned char max_transfer_sectors,
    block_device_read_fn read, block_device_write_fn write, void *context)
{
    if (device == 0) {
        return 0;
    }
    block_device_reset(device);
    if (sector_count == 0U || sector_size != BLOCK_DEVICE_SECTOR_SIZE ||
        max_transfer_sectors == 0U ||
        max_transfer_sectors > BLOCK_DEVICE_TRANSFER_MAX || read == 0) {
        return 0;
    }
    device->sector_count = sector_count;
    device->sector_size = sector_size;
    device->max_transfer_sectors = max_transfer_sectors;
    device->read = read;
    device->write = write;
    device->context = context;
    device->magic = BLOCK_DEVICE_MAGIC;
    return 1;
}

int block_device_is_ready(const struct block_device *device)
{
    return device != 0 && device->magic == BLOCK_DEVICE_MAGIC &&
        device->sector_count != 0U &&
        device->sector_size == BLOCK_DEVICE_SECTOR_SIZE &&
        device->max_transfer_sectors != 0U &&
        device->max_transfer_sectors <= BLOCK_DEVICE_TRANSFER_MAX &&
        device->read != 0;
}

int block_device_can_write(const struct block_device *device)
{
    return block_device_is_ready(device) && device->write != 0;
}

unsigned int block_device_sector_count(const struct block_device *device)
{
    return block_device_is_ready(device) ? device->sector_count : 0U;
}

static int request_is_valid(const struct block_device *device, unsigned int lba,
    unsigned char count, const void *buffer)
{
    return block_device_is_ready(device) && buffer != 0 && count != 0U &&
        count <= device->max_transfer_sectors && lba < device->sector_count &&
        (unsigned int)count <= device->sector_count - lba;
}

int block_device_read(const struct block_device *device, unsigned int lba,
    unsigned char count, void *buffer)
{
    if (!request_is_valid(device, lba, count, buffer)) {
        return 0;
    }
    return device->read(device->context, lba, count, buffer);
}

int block_device_write(const struct block_device *device, unsigned int lba,
    unsigned char count, const void *buffer)
{
    if (!request_is_valid(device, lba, count, buffer) || device->write == 0) {
        return 0;
    }
    return device->write(device->context, lba, count, buffer);
}
