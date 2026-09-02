#include "persistent.h"
#include "ata.h"
#include "journal.h"
#include "panic.h"
#include "ramfs.h"
#include "serial.h"
#include "vfs.h"

#define PERSISTENT_JOURNAL_REGION_SECTORS 65U

static int persistent_enabled;
static unsigned int persistent_region_start;
static unsigned int persistent_next_sequence;
static unsigned int persistent_replay_records;

static int build_entry(unsigned int operation, unsigned int sequence,
    const char *name, const char *content, unsigned int content_length,
    struct journal_entry *entry)
{
    unsigned char sector[JOURNAL_SECTOR_SIZE];

    return entry != 0 && journal_encode(sector, operation, sequence, name,
        content, content_length) && journal_decode(sector, entry);
}

static int advance_sequence(void)
{
    if (persistent_next_sequence == 0xFFFFFFFFU) {
        persistent_next_sequence = 0U;
    } else {
        persistent_next_sequence++;
    }
    return persistent_next_sequence != 0U;
}

int persistent_ramfs_init(void)
{
    unsigned int data_sectors;
    unsigned char superblock[JOURNAL_SECTOR_SIZE];

    persistent_enabled = 0;
    persistent_region_start = 0U;
    persistent_next_sequence = 0U;
    persistent_replay_records = 0U;
    ata_disable_transactional_writes();

    if (!ata_present() || ata_sector_count() <= PERSISTENT_JOURNAL_REGION_SECTORS) {
        return 1;
    }
    persistent_region_start = ata_sector_count() - PERSISTENT_JOURNAL_REGION_SECTORS;
    if (!vfs_journal_region_available(persistent_region_start,
        PERSISTENT_JOURNAL_REGION_SECTORS) ||
        !ata_read_sectors(persistent_region_start, 1U, superblock) ||
        !journal_superblock_decode(superblock, &data_sectors)) {
        /* No formatted journal is a supported read-only/volatile mode. */
        return 1;
    }
    if (data_sectors + 1U > PERSISTENT_JOURNAL_REGION_SECTORS ||
        !journal_replay(ata_read_sectors, persistent_region_start,
            PERSISTENT_JOURNAL_REGION_SECTORS, ramfs_apply_journal_entry,
            &persistent_replay_records)) {
        return 0;
    }
    if (!journal_next_sequence(ata_read_sectors, persistent_region_start,
        PERSISTENT_JOURNAL_REGION_SECTORS, &persistent_next_sequence)) {
        /* A valid log whose sequence space is exhausted remains readable;
           simply keep the persistent write capability disabled. */
        return 1;
    }
    /* The ATA driver accepts writes only inside this validated journal
       region; FAT sectors can never be reached through the write callback. */
    if (!ata_enable_transactional_writes(persistent_region_start,
        PERSISTENT_JOURNAL_REGION_SECTORS)) {
        return 0;
    }
    persistent_enabled = 1;
    return 1;
}

int persistent_ramfs_is_enabled(void)
{
    return persistent_enabled;
}

unsigned int persistent_ramfs_replay_count(void)
{
    return persistent_replay_records;
}

int persistent_ramfs_write_file(const char *name, const char *contents)
{
    unsigned int content_length;
    struct journal_entry entry;

    if (!persistent_enabled || persistent_next_sequence == 0U ||
        !ramfs_can_write_file(name, contents, &content_length) ||
        !build_entry(JOURNAL_OPERATION_WRITE, persistent_next_sequence, name,
            contents, content_length, &entry)) {
        return 0;
    }
    if (!journal_append(ata_read_sectors, ata_write_sectors,
        persistent_region_start, PERSISTENT_JOURNAL_REGION_SECTORS,
        JOURNAL_OPERATION_WRITE, persistent_next_sequence, name, contents,
        content_length)) {
        serial_write("EfesOS: persistent RAMFS write journal append failed status=");
        serial_write_hex(ata_last_status());
        serial_write(".\n");
        return 0;
    }
    if (!ramfs_apply_journal_entry(&entry)) {
        kernel_panic("Committed persistent RAMFS write could not be applied.");
    }
    serial_write("EfesOS: persistent RAMFS write committed.\n");
    return advance_sequence() || persistent_next_sequence == 0U;
}

int persistent_ramfs_remove_file(const char *name)
{
    struct journal_entry entry;

    if (!persistent_enabled || persistent_next_sequence == 0U ||
        ramfs_file_contents(name) == 0 ||
        !build_entry(JOURNAL_OPERATION_REMOVE, persistent_next_sequence, name,
            0, 0U, &entry)) {
        return 0;
    }
    if (!journal_append(ata_read_sectors, ata_write_sectors,
        persistent_region_start, PERSISTENT_JOURNAL_REGION_SECTORS,
        JOURNAL_OPERATION_REMOVE, persistent_next_sequence, name, 0, 0U)) {
        serial_write("EfesOS: persistent RAMFS remove journal append failed status=");
        serial_write_hex(ata_last_status());
        serial_write(".\n");
        return 0;
    }
    if (!ramfs_apply_journal_entry(&entry)) {
        kernel_panic("Committed persistent RAMFS remove could not be applied.");
    }
    serial_write("EfesOS: persistent RAMFS remove committed.\n");
    return advance_sequence() || persistent_next_sequence == 0U;
}
