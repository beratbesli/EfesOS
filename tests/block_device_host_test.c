#include <stdio.h>

#include "block_device.h"

#define TEST_SECTORS 16U

struct test_context {
    unsigned int read_calls;
    unsigned int write_calls;
    int fail_reads;
    unsigned char disk[TEST_SECTORS * BLOCK_DEVICE_SECTOR_SIZE];
};

static int test_read(void *opaque, unsigned int lba, unsigned char count,
    void *buffer)
{
    struct test_context *context = (struct test_context *)opaque;
    unsigned int index;
    unsigned int bytes = (unsigned int)count * BLOCK_DEVICE_SECTOR_SIZE;

    context->read_calls++;
    if (context->fail_reads) {
        return 0;
    }
    for (index = 0U; index < bytes; index++) {
        ((unsigned char *)buffer)[index] =
            context->disk[(lba * BLOCK_DEVICE_SECTOR_SIZE) + index];
    }
    return 1;
}

static int test_write(void *opaque, unsigned int lba, unsigned char count,
    const void *buffer)
{
    struct test_context *context = (struct test_context *)opaque;
    unsigned int index;
    unsigned int bytes = (unsigned int)count * BLOCK_DEVICE_SECTOR_SIZE;

    context->write_calls++;
    for (index = 0U; index < bytes; index++) {
        context->disk[(lba * BLOCK_DEVICE_SECTOR_SIZE) + index] =
            ((const unsigned char *)buffer)[index];
    }
    return 1;
}

int main(void)
{
    struct block_device device;
    struct test_context context = {0U, 0U, 0, {0U}};
    unsigned char sector[BLOCK_DEVICE_SECTOR_SIZE] = {0U};

    block_device_reset(&device);
    if (block_device_is_ready(&device) || block_device_can_write(&device) ||
        block_device_sector_count(&device) != 0U ||
        block_device_configure(0, TEST_SECTORS, BLOCK_DEVICE_SECTOR_SIZE, 1U,
            test_read, 0, &context) ||
        block_device_configure(&device, 0U, BLOCK_DEVICE_SECTOR_SIZE, 1U,
            test_read, 0, &context) ||
        block_device_configure(&device, TEST_SECTORS, 4096U, 1U,
            test_read, 0, &context) ||
        block_device_configure(&device, TEST_SECTORS, BLOCK_DEVICE_SECTOR_SIZE,
            0U, test_read, 0, &context) ||
        block_device_configure(&device, TEST_SECTORS, BLOCK_DEVICE_SECTOR_SIZE,
            BLOCK_DEVICE_TRANSFER_MAX + 1U, test_read, 0, &context) ||
        block_device_configure(&device, TEST_SECTORS, BLOCK_DEVICE_SECTOR_SIZE,
            1U, 0, 0, &context)) {
        return 1;
    }
    if (!block_device_configure(&device, TEST_SECTORS,
            BLOCK_DEVICE_SECTOR_SIZE, 4U, test_read, 0, &context) ||
        !block_device_is_ready(&device) || block_device_can_write(&device) ||
        block_device_sector_count(&device) != TEST_SECTORS) {
        return 2;
    }
    context.disk[15U * BLOCK_DEVICE_SECTOR_SIZE] = 0xA5U;
    if (!block_device_read(&device, 15U, 1U, sector) || sector[0] != 0xA5U ||
        context.read_calls != 1U ||
        block_device_read(&device, 15U, 2U, sector) ||
        block_device_read(&device, 0U, 0U, sector) ||
        block_device_read(&device, 0U, 1U, 0) ||
        block_device_read(&device, 0U, 5U, sector) ||
        context.read_calls != 1U) {
        return 3;
    }
    context.fail_reads = 1;
    if (block_device_read(&device, 0U, 1U, sector) ||
        context.read_calls != 2U || block_device_write(&device, 0U, 1U, sector)) {
        return 4;
    }
    context.fail_reads = 0;
    sector[0] = 0x5AU;
    if (!block_device_configure(&device, TEST_SECTORS,
            BLOCK_DEVICE_SECTOR_SIZE, 4U, test_read, test_write, &context) ||
        !block_device_can_write(&device) ||
        !block_device_write(&device, 15U, 1U, sector) ||
        context.write_calls != 1U ||
        context.disk[15U * BLOCK_DEVICE_SECTOR_SIZE] != 0x5AU ||
        block_device_write(&device, 15U, 2U, sector) ||
        context.write_calls != 1U) {
        return 5;
    }
    block_device_reset(&device);
    if (block_device_read(&device, 0U, 1U, sector) ||
        block_device_write(&device, 0U, 1U, sector)) {
        return 6;
    }
    puts("Block device host self-test passed.");
    return 0;
}
