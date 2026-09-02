#ifndef EFESOS_BLOCK_DEVICE_H
#define EFESOS_BLOCK_DEVICE_H

#define BLOCK_DEVICE_SECTOR_SIZE 512U
#define BLOCK_DEVICE_TRANSFER_MAX 128U

typedef int (*block_device_read_fn)(void *context, unsigned int lba,
    unsigned char count, void *buffer);
typedef int (*block_device_write_fn)(void *context, unsigned int lba,
    unsigned char count, const void *buffer);

struct block_device {
    unsigned int magic;
    unsigned int sector_count;
    unsigned short sector_size;
    unsigned char max_transfer_sectors;
    block_device_read_fn read;
    block_device_write_fn write;
    void *context;
};

void block_device_reset(struct block_device *device);
int block_device_configure(struct block_device *device, unsigned int sector_count,
    unsigned short sector_size, unsigned char max_transfer_sectors,
    block_device_read_fn read, block_device_write_fn write, void *context);
int block_device_is_ready(const struct block_device *device);
int block_device_can_write(const struct block_device *device);
unsigned int block_device_sector_count(const struct block_device *device);
int block_device_read(const struct block_device *device, unsigned int lba,
    unsigned char count, void *buffer);
int block_device_write(const struct block_device *device, unsigned int lba,
    unsigned char count, const void *buffer);

#endif
