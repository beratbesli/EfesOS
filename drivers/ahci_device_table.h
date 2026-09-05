#ifndef EFESOS_AHCI_DEVICE_TABLE_H
#define EFESOS_AHCI_DEVICE_TABLE_H

#include "block_device.h"
#include "ahci_layout.h"

#define AHCI_DEVICE_TABLE_MAX 8U

struct ahci_device_record {
    struct block_device block_device;
    uint16_t identify[256];
    uint32_t sector_count;
    unsigned int validation_generation;
    unsigned int controller_index;
    unsigned int port;
    int lba48_supported;
};

struct ahci_device_table {
    unsigned int magic;
    unsigned int count;
    struct ahci_device_record records[AHCI_DEVICE_TABLE_MAX];
};

void ahci_device_table_reset(struct ahci_device_table *table);
int ahci_device_table_add(struct ahci_device_table *table,
    unsigned int controller_index, unsigned int port,
    unsigned int validation_generation, const uint16_t identify[256],
    block_device_read_fn read);
unsigned int ahci_device_table_count(const struct ahci_device_table *table);
struct ahci_device_record *ahci_device_table_record_at(
    struct ahci_device_table *table, unsigned int index);
const struct ahci_device_record *ahci_device_table_record_at_const(
    const struct ahci_device_table *table, unsigned int index);
const struct block_device *ahci_device_table_block_at(
    const struct ahci_device_table *table, unsigned int index);

#endif
