#include "ahci_device_table.h"

#define AHCI_DEVICE_TABLE_MAGIC 0x41484454U

static void clear_record(struct ahci_device_record *record)
{
    unsigned int index;

    block_device_reset(&record->block_device);
    for (index = 0U; index < 256U; index++) {
        record->identify[index] = 0U;
    }
    record->sector_count = 0U;
    record->controller_index = ~0U;
    record->port = ~0U;
    record->lba48_supported = 0;
}

static int table_is_valid(const struct ahci_device_table *table)
{
    return table != 0 && table->magic == AHCI_DEVICE_TABLE_MAGIC &&
        table->count <= AHCI_DEVICE_TABLE_MAX;
}

void ahci_device_table_reset(struct ahci_device_table *table)
{
    unsigned int index;

    if (table == 0) {
        return;
    }
    table->magic = AHCI_DEVICE_TABLE_MAGIC;
    table->count = 0U;
    for (index = 0U; index < AHCI_DEVICE_TABLE_MAX; index++) {
        clear_record(&table->records[index]);
    }
}

int ahci_device_table_add(struct ahci_device_table *table,
    unsigned int controller_index, unsigned int port,
    const uint16_t identify[256], block_device_read_fn read)
{
    struct ahci_device_record *record;
    uint32_t sector_count;
    int lba48_supported;
    unsigned int index;

    if (!table_is_valid(table) || table->count == AHCI_DEVICE_TABLE_MAX ||
        controller_index == ~0U || port >= 32U || identify == 0 || read == 0 ||
        !ahci_identify_capacity(identify, &sector_count, &lba48_supported)) {
        return 0;
    }
    for (index = 0U; index < table->count; index++) {
        const struct ahci_device_record *existing = &table->records[index];

        if (existing->controller_index == controller_index &&
            existing->port == port) {
            return 0;
        }
    }

    record = &table->records[table->count];
    clear_record(record);
    record->sector_count = sector_count;
    record->controller_index = controller_index;
    record->port = port;
    record->lba48_supported = lba48_supported;
    for (index = 0U; index < 256U; index++) {
        record->identify[index] = identify[index];
    }
    if (!block_device_configure(&record->block_device, sector_count,
            AHCI_SECTOR_SIZE, AHCI_MAX_TRANSFER_SECTORS, read, 0, record)) {
        clear_record(record);
        return 0;
    }
    table->count++;
    return 1;
}

unsigned int ahci_device_table_count(const struct ahci_device_table *table)
{
    return table_is_valid(table) ? table->count : 0U;
}

struct ahci_device_record *ahci_device_table_record_at(
    struct ahci_device_table *table, unsigned int index)
{
    if (!table_is_valid(table) || index >= table->count ||
        !block_device_is_ready(&table->records[index].block_device)) {
        return 0;
    }
    return &table->records[index];
}

const struct ahci_device_record *ahci_device_table_record_at_const(
    const struct ahci_device_table *table, unsigned int index)
{
    if (!table_is_valid(table) || index >= table->count ||
        !block_device_is_ready(&table->records[index].block_device)) {
        return 0;
    }
    return &table->records[index];
}

const struct block_device *ahci_device_table_block_at(
    const struct ahci_device_table *table, unsigned int index)
{
    const struct ahci_device_record *record =
        ahci_device_table_record_at_const(table, index);

    return record != 0 ? &record->block_device : 0;
}
