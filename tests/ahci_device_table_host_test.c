#include <stdio.h>

#include "ahci_device_table.h"

static unsigned int read_calls;

static int fake_read(void *context, uint64_t lba,
    uint32_t count, void *buffer)
{
    const struct ahci_device_record *record =
        (const struct ahci_device_record *)context;
    unsigned char *bytes = (unsigned char *)buffer;

    if (record == 0 || count != 1U || lba >= record->sector_count) {
        return 0;
    }
    bytes[0] = (unsigned char)record->port;
    read_calls++;
    return 1;
}

static void make_identify(uint16_t identify[256], uint64_t sectors)
{
    unsigned int index;

    for (index = 0U; index < 256U; index++) {
        identify[index] = 0U;
    }
    identify[49] = 0x0200U;
    identify[83] = 0x0400U;
    identify[100] = (uint16_t)sectors;
    identify[101] = (uint16_t)(sectors >> 16U);
    identify[102] = (uint16_t)(sectors >> 32U);
    identify[103] = (uint16_t)(sectors >> 48U);
}

int main(void)
{
    struct ahci_device_table table;
    uint16_t identify[256];
    unsigned char sector[AHCI_SECTOR_SIZE];
    unsigned int index;

    ahci_device_table_reset(&table);
    make_identify(identify, 8192U);
    if (ahci_device_table_count(&table) != 0U ||
        ahci_device_table_record_at(&table, 0U) != 0 ||
        ahci_device_table_block_at(&table, 0U) != 0 ||
        ahci_device_table_add(0, 0U, 0U, 1U, identify, fake_read) ||
        ahci_device_table_add(&table, ~0U, 0U, 1U, identify, fake_read) ||
        ahci_device_table_add(&table, 0U, 32U, 1U, identify, fake_read) ||
        ahci_device_table_add(&table, 0U, 0U, 0U, identify, fake_read) ||
        ahci_device_table_add(&table, 0U, 0U, 1U, 0, fake_read) ||
        ahci_device_table_add(&table, 0U, 0U, 1U, identify, 0)) {
        return 1;
    }

    identify[49] = 0U;
    if (ahci_device_table_add(&table, 0U, 0U, 1U, identify, fake_read)) {
        return 2;
    }
    make_identify(identify, 8192U);
    if (!ahci_device_table_add(&table, 1U, 2U, 7U, identify, fake_read) ||
        ahci_device_table_count(&table) != 1U ||
        ahci_device_table_add(&table, 1U, 2U, 7U, identify, fake_read)) {
        return 3;
    }
    if (ahci_device_table_record_at(&table, 0U)->controller_index != 1U ||
        ahci_device_table_record_at(&table, 0U)->port != 2U ||
        ahci_device_table_record_at(&table, 0U)->sector_count != 8192U ||
        ahci_device_table_record_at(&table, 0U)->validation_generation != 7U ||
        !ahci_device_table_record_at(&table, 0U)->lba48_supported ||
        !block_device_read(ahci_device_table_block_at(&table, 0U),
            8191U, 1U, sector) || sector[0] != 2U || read_calls != 1U) {
        return 4;
    }

    ahci_device_table_reset(&table);
    make_identify(identify, 0x0000ABCD12345678ULL);
    if (!ahci_device_table_add(&table, 1U, 2U, 7U, identify, fake_read) ||
        ahci_device_table_record_at(&table, 0U)->sector_count !=
            0x0000ABCD12345678ULL ||
        !block_device_read(ahci_device_table_block_at(&table, 0U),
            0x0000ABCD12345677ULL, 1U, sector) ||
        block_device_read(ahci_device_table_block_at(&table, 0U),
            0x0000ABCD12345678ULL, 1U, sector)) {
        return 5;
    }

    for (index = 1U; index < AHCI_DEVICE_TABLE_MAX; index++) {
        make_identify(identify, 8192U + index);
        if (!ahci_device_table_add(&table, 1U, index + 2U, 7U,
                identify, fake_read)) {
            return 6;
        }
    }
    make_identify(identify, 16384U);
    if (ahci_device_table_count(&table) != AHCI_DEVICE_TABLE_MAX ||
        ahci_device_table_add(&table, 2U, 0U, 7U, identify, fake_read) ||
        ahci_device_table_block_at(&table, AHCI_DEVICE_TABLE_MAX) != 0) {
        return 7;
    }

    ahci_device_table_reset(&table);
    if (ahci_device_table_count(&table) != 0U ||
        ahci_device_table_record_at_const(&table, 0U) != 0) {
        return 8;
    }
    puts("AHCI device table host tests passed.");
    return 0;
}
