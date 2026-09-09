#ifndef EFESOS_BLOCK_DEVICE_H
#define EFESOS_BLOCK_DEVICE_H

#include <stdint.h>

#define BLOCK_DEVICE_SECTOR_SIZE 512U
#define BLOCK_DEVICE_TRANSFER_MAX 128U

/* Capacity is a sector count; requests use an absolute 64-bit LBA.  Drivers
   with a narrower wire protocol must reject unsupported requests themselves. */
typedef int (*block_device_read_fn)(void *context, uint64_t lba,
    uint32_t count, void *buffer);
typedef int (*block_device_write_fn)(void *context, uint64_t lba,
    uint32_t count, const void *buffer);

struct block_device {
    unsigned int magic;
    uint64_t sector_count;
    unsigned short sector_size;
    uint32_t max_transfer_sectors;
    block_device_read_fn read;
    block_device_write_fn write;
    void *context;
};

void block_device_reset(struct block_device *device);
int block_device_configure(struct block_device *device, uint64_t sector_count,
    unsigned short sector_size, uint32_t max_transfer_sectors,
    block_device_read_fn read, block_device_write_fn write, void *context);
int block_device_is_ready(const struct block_device *device);
int block_device_can_write(const struct block_device *device);
uint64_t block_device_sector_count(const struct block_device *device);
int block_device_read(const struct block_device *device, uint64_t lba,
    uint32_t count, void *buffer);
int block_device_write(const struct block_device *device, uint64_t lba,
    uint32_t count, const void *buffer);

#endif
