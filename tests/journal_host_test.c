#include <stdio.h>
#include <string.h>

#include "journal.h"

static unsigned char disk[(JOURNAL_MAX_DATA_SECTORS + 1U) * JOURNAL_SECTOR_SIZE];
static unsigned int applied_count;
static unsigned int write_count;
static unsigned int fail_write_number;

static int read_disk(unsigned int lba, unsigned char count, void *buffer)
{
    if (count == 0U || lba >= JOURNAL_MAX_DATA_SECTORS + 1U ||
        count > JOURNAL_MAX_DATA_SECTORS + 1U - lba) {
        return 0;
    }
    memcpy(buffer, disk + (lba * JOURNAL_SECTOR_SIZE), count * JOURNAL_SECTOR_SIZE);
    return 1;
}

static int write_disk(unsigned int lba, unsigned char count, const void *buffer)
{
    if (count == 0U || lba >= JOURNAL_MAX_DATA_SECTORS + 1U ||
        count > JOURNAL_MAX_DATA_SECTORS + 1U - lba) {
        return 0;
    }
    write_count++;
    if (fail_write_number != 0U && write_count == fail_write_number) {
        return 0;
    }
    memcpy(disk + (lba * JOURNAL_SECTOR_SIZE), buffer, count * JOURNAL_SECTOR_SIZE);
    return 1;
}

static int apply_entry(const struct journal_entry *entry)
{
    if (entry == 0 || entry->sequence == 0U) {
        return 0;
    }
    applied_count++;
    return 1;
}

int main(void)
{
    unsigned char sector[JOURNAL_SECTOR_SIZE];
    struct journal_entry entry;
    unsigned int index;

    if (!journal_encode(sector, JOURNAL_OPERATION_WRITE, 7U, "CONFIG",
        "safe", 4U) || !journal_decode(sector, &entry) ||
        entry.operation != JOURNAL_OPERATION_WRITE || entry.sequence != 7U ||
        entry.name_length != 6U || entry.content_length != 4U ||
        memcmp(entry.name, "CONFIG", 6U) != 0 ||
        memcmp(entry.content, "safe", 4U) != 0) {
        return 1;
    }

    for (index = 0U; index < JOURNAL_SECTOR_SIZE; index++) {
        unsigned char original = sector[index];
        sector[index] ^= 0x01U;
        if (journal_decode(sector, &entry)) {
            return 2;
        }
        sector[index] = original;
    }
    if (journal_encode(sector, JOURNAL_OPERATION_REMOVE, 8U, "CONFIG", 0, 0U) == 0 ||
        !journal_decode(sector, &entry) || entry.operation != JOURNAL_OPERATION_REMOVE) {
        return 3;
    }
    memset(disk, 0, sizeof(disk));
    if (!journal_superblock_encode(disk, 4U) ||
        !journal_encode(disk + JOURNAL_SECTOR_SIZE, JOURNAL_OPERATION_WRITE, 1U,
            "A", "one", 3U) ||
        !journal_encode(disk + (2U * JOURNAL_SECTOR_SIZE), JOURNAL_OPERATION_REMOVE,
            2U, "A", 0, 0U)) {
        return 4;
    }
    applied_count = 0U;
    if (!journal_replay(read_disk, 0U, 5U, apply_entry, 0) || applied_count != 2U) {
        return 5;
    }
    disk[JOURNAL_SECTOR_SIZE + 1U] ^= 0x01U;
    applied_count = 0U;
    if (journal_replay(read_disk, 0U, 5U, apply_entry, 0) || applied_count != 0U) {
        return 6;
    }
    memset(disk, 0, sizeof(disk));
    if (!journal_superblock_encode(disk, 4U)) {
        return 7;
    }
    write_count = 0U;
    fail_write_number = 0U;
    if (!journal_append(read_disk, write_disk, 0U, 5U, JOURNAL_OPERATION_WRITE,
        1U, "A", "one", 3U) || !journal_append(read_disk, write_disk, 0U, 5U,
        JOURNAL_OPERATION_REMOVE, 2U, "A", 0, 0U)) {
        return 8;
    }
    applied_count = 0U;
    if (!journal_replay(read_disk, 0U, 5U, apply_entry, 0) || applied_count != 2U ||
        write_count != 4U) {
        return 9;
    }
    memset(disk, 0, sizeof(disk));
    if (!journal_superblock_encode(disk, 4U)) {
        return 10;
    }
    write_count = 0U;
    fail_write_number = 2U;
    if (journal_append(read_disk, write_disk, 0U, 5U, JOURNAL_OPERATION_WRITE,
        1U, "TORN", "bad", 3U)) {
        return 11;
    }
    fail_write_number = 0U;
    applied_count = 0U;
    if (journal_replay(read_disk, 0U, 5U, apply_entry, 0) || applied_count != 0U) {
        return 12;
    }
    puts("Journal host self-test passed.");
    return 0;
}
