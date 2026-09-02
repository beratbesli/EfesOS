#include <stdio.h>
#include <stdlib.h>

#include "ata.h"
#include "journal.h"
#include "persistent.h"
#include "ramfs.h"

#define DISK_SECTORS 128U
#define JOURNAL_START (DISK_SECTORS - 65U)

static unsigned char disk[DISK_SECTORS * JOURNAL_SECTOR_SIZE];
static unsigned int enabled_start;
static unsigned int enabled_count;

static void clear_disk(void)
{
    unsigned int index;

    for (index = 0U; index < sizeof(disk); index++) {
        disk[index] = 0U;
    }
}

int ata_present(void)
{
    return 1;
}

unsigned int ata_sector_count(void)
{
    return DISK_SECTORS;
}

int ata_read_sectors(unsigned int lba, unsigned char count, void *buffer)
{
    if (count == 0U || lba >= DISK_SECTORS || count > DISK_SECTORS - lba) {
        return 0;
    }
    for (unsigned int index = 0U; index < (unsigned int)count * JOURNAL_SECTOR_SIZE; index++) {
        ((unsigned char *)buffer)[index] = disk[(lba * JOURNAL_SECTOR_SIZE) + index];
    }
    return 1;
}

int ata_write_sectors(unsigned int lba, unsigned char count, const void *buffer)
{
    if (count == 0U || lba < enabled_start ||
        lba - enabled_start >= enabled_count ||
        (unsigned int)count > enabled_count - (lba - enabled_start) ||
        lba >= DISK_SECTORS || count > DISK_SECTORS - lba) {
        return 0;
    }
    for (unsigned int index = 0U; index < (unsigned int)count * JOURNAL_SECTOR_SIZE; index++) {
        disk[(lba * JOURNAL_SECTOR_SIZE) + index] = ((const unsigned char *)buffer)[index];
    }
    return 1;
}

int ata_enable_transactional_writes(unsigned int start_lba, unsigned int sector_count)
{
    enabled_start = start_lba;
    enabled_count = sector_count;
    return start_lba < DISK_SECTORS && sector_count != 0U &&
        sector_count <= DISK_SECTORS - start_lba;
}

void ata_disable_transactional_writes(void)
{
    enabled_start = 0U;
    enabled_count = 0U;
}

uint8_t ata_last_status(void)
{
    return 0U;
}

void serial_write(const char *text)
{
    (void)text;
}

void serial_write_hex(unsigned int value)
{
    (void)value;
}

void *kmalloc(unsigned int size)
{
    return malloc(size);
}

void kfree(void *pointer)
{
    free(pointer);
}

int vfs_journal_region_available(unsigned int start_lba, unsigned int sector_count)
{
    return start_lba == JOURNAL_START && sector_count == 65U;
}

void kernel_panic(const char *message)
{
    (void)message;
    abort();
}

static void seed_journal(void)
{
    if (!journal_superblock_encode(disk + JOURNAL_START * JOURNAL_SECTOR_SIZE, 64U) ||
        !journal_encode(disk + (JOURNAL_START + 1U) * JOURNAL_SECTOR_SIZE,
            JOURNAL_OPERATION_WRITE, 1U, "PERSIST", "seed", 4U)) {
        abort();
    }
}

int main(void)
{
    const char *value;

    clear_disk();
    ramfs_init();
    if (!ramfs_write_file("VOLATILE", "x") ||
        !persistent_ramfs_init() || persistent_ramfs_format()) {
        return 1;
    }
    ramfs_init();
    if (!persistent_ramfs_init() || persistent_ramfs_is_enabled() ||
        !persistent_ramfs_format() || !persistent_ramfs_is_enabled() ||
        !persistent_ramfs_write_file("FORMATTED", "ok")) {
        return 2;
    }
    ramfs_init();
    if (!persistent_ramfs_init() ||
        !ramfs_file_contents("FORMATTED")) {
        return 3;
    }

    clear_disk();
    seed_journal();
    ramfs_init();
    if (!persistent_ramfs_init() || !persistent_ramfs_is_enabled() ||
        persistent_ramfs_replay_count() != 1U ||
        !ramfs_file_contents("PERSIST")) {
        return 4;
    }
    if (!persistent_ramfs_write_file("NEW", "durable") ||
        !persistent_ramfs_remove_file("PERSIST") ||
        ramfs_file_contents("PERSIST") != 0) {
        return 5;
    }

    /* Reinitialize the volatile view to model a reboot and replay the disk. */
    ramfs_init();
    if (!persistent_ramfs_init() || persistent_ramfs_replay_count() != 3U ||
        ramfs_file_contents("PERSIST") != 0 ||
        (value = ramfs_file_contents("NEW")) == 0 ||
        value[0] != 'd') {
        return 6;
    }
    {
        unsigned char sector[JOURNAL_SECTOR_SIZE] = {0};

        if (!ata_enable_transactional_writes(10U, 2U) ||
            ata_write_sectors(12U, 1U, sector)) {
            return 7;
        }
    }
    puts("Persistent RAMFS host self-test passed.");
    return 0;
}
