#include <stdio.h>

#include "fat.h"

#define SECTOR_SIZE 512U
#define SECTOR_COUNT 8192U

static unsigned char disk[SECTOR_SIZE * SECTOR_COUNT];

static void put16(unsigned int offset, unsigned int value)
{
    disk[offset] = (unsigned char)value;
    disk[offset + 1U] = (unsigned char)(value >> 8U);
}

static void put32(unsigned int offset, unsigned int value)
{
    put16(offset, value);
    put16(offset + 2U, value >> 16U);
}

static int read_fixture(unsigned int lba, unsigned char count, void *buffer)
{
    unsigned int index;
    unsigned int bytes = (unsigned int)count * SECTOR_SIZE;
    unsigned int offset = lba * SECTOR_SIZE;

    if (count == 0U || lba >= SECTOR_COUNT || count > SECTOR_COUNT - lba) {
        return 0;
    }
    for (index = 0; index < bytes; index++) {
        ((unsigned char *)buffer)[index] = disk[offset + index];
    }
    return 1;
}

static int read_fixed_boot(unsigned int lba, unsigned char count, void *buffer)
{
    unsigned int index;

    (void)lba;
    if (count != 1U) {
        return 0;
    }
    for (index = 0; index < SECTOR_SIZE; index++) {
        ((unsigned char *)buffer)[index] = disk[index];
    }
    return 1;
}

static void build_fixture(void)
{
    unsigned int index;
    const char *name = "HELLO   TXT";
    const char *contents = "EfesOS disk!\r\n";

    for (index = 0; index < sizeof(disk); index++) {
        disk[index] = 0;
    }
    put16(11, SECTOR_SIZE);
    disk[13] = 1;
    put16(14, 1);
    disk[16] = 2;
    put16(17, 32);
    put16(19, SECTOR_COUNT);
    disk[21] = 0xF8;
    put16(22, 32);
    disk[510] = 0x55;
    disk[511] = 0xAA;
    for (index = 0; index < 11U; index++) {
        disk[(65U * SECTOR_SIZE) + index] = (unsigned char)name[index];
    }
    disk[(65U * SECTOR_SIZE) + 11U] = 0x20;
    put16((65U * SECTOR_SIZE) + 26U, 2);
    put32((65U * SECTOR_SIZE) + 28U, 14);
    disk[512] = 0xF8;
    disk[513] = 0xFF;
    disk[514] = 0xFF;
    disk[515] = 0xFF;
    disk[516] = 0xF8;
    disk[517] = 0xFF;
    disk[33U * SECTOR_SIZE + 512U] = 0xF8;
    disk[33U * SECTOR_SIZE + 513U] = 0xFF;
    disk[33U * SECTOR_SIZE + 514U] = 0xFF;
    disk[33U * SECTOR_SIZE + 515U] = 0xFF;
    disk[33U * SECTOR_SIZE + 516U] = 0xF8;
    disk[33U * SECTOR_SIZE + 517U] = 0xFF;
    for (index = 0; index < 14U; index++) {
        disk[(67U * SECTOR_SIZE) + index] = (unsigned char)contents[index];
    }
}

int main(void)
{
    struct fat_volume volume;
    char name[13];
    char contents[32];
    unsigned int size;

    build_fixture();
    if (!fat_mount(&volume, read_fixture, 0) || fat_file_count(&volume) != 1U ||
        !fat_file_name(&volume, 0, name, sizeof(name)) ||
        name[0] != 'H' || name[1] != 'E' || name[2] != 'L' ||
        !fat_read_file(&volume, "hello.txt", contents, sizeof(contents), &size) ||
        size != 14U || contents[0] != 'E' || contents[13] != '\n') {
        return 1;
    }
    build_fixture();
    put16(11, 256);
    if (fat_mount(&volume, read_fixture, 0) || fat_last_error() != 4U) {
        return 1;
    }
    build_fixture();
    disk[13] = 129;
    if (fat_mount(&volume, read_fixture, 0) || fat_last_error() != 4U) {
        return 1;
    }
    build_fixture();
    put16(17, 1);
    disk[(65U * SECTOR_SIZE) + 32U] = 'E';
    disk[(65U * SECTOR_SIZE) + 33U] = 'V';
    disk[(65U * SECTOR_SIZE) + 34U] = 'I';
    disk[(65U * SECTOR_SIZE) + 35U] = 'L';
    disk[(65U * SECTOR_SIZE) + 40U] = 'T';
    disk[(65U * SECTOR_SIZE) + 41U] = 'X';
    disk[(65U * SECTOR_SIZE) + 42U] = 'T';
    disk[(65U * SECTOR_SIZE) + 43U] = 0x20;
    if (!fat_mount(&volume, read_fixture, 0) || fat_file_count(&volume) != 1U ||
        fat_file_name(&volume, 1U, name, sizeof(name))) {
        return 1;
    }
    build_fixture();
    if (fat_mount(&volume, read_fixed_boot, 0xFFFFFFF0U) || fat_last_error() != 7U) {
        return 1;
    }
    puts("FAT host self-test passed.");
    return 0;
}
