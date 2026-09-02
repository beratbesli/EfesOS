#include "journal.h"

#define JOURNAL_MAGIC 0x314A5346U
#define JOURNAL_VERSION 1U
#define JOURNAL_COMMIT 0xC0DE17EDU
#define JOURNAL_SUPERBLOCK_MAGIC 0x314A5346U
#define JOURNAL_SUPERBLOCK_VERSION 1U
#define JOURNAL_HEADER_SIZE 20U
#define JOURNAL_PAYLOAD_SIZE (JOURNAL_NAME_MAX + JOURNAL_CONTENT_MAX)
#define JOURNAL_COMMIT_OFFSET (JOURNAL_SECTOR_SIZE - sizeof(unsigned int))

static unsigned int read_u16(const unsigned char *data, unsigned int offset)
{
    return (unsigned int)data[offset] | ((unsigned int)data[offset + 1U] << 8U);
}

static unsigned int read_u32(const unsigned char *data, unsigned int offset)
{
    return (unsigned int)data[offset] | ((unsigned int)data[offset + 1U] << 8U) |
        ((unsigned int)data[offset + 2U] << 16U) | ((unsigned int)data[offset + 3U] << 24U);
}

static void write_u16(unsigned char *data, unsigned int offset, unsigned int value)
{
    data[offset] = (unsigned char)(value & 0xFFU);
    data[offset + 1U] = (unsigned char)((value >> 8U) & 0xFFU);
}

static void write_u32(unsigned char *data, unsigned int offset, unsigned int value)
{
    write_u16(data, offset, value & 0xFFFFU);
    write_u16(data, offset + 2U, value >> 16U);
}

static unsigned int crc32_update(unsigned int crc, const unsigned char *data,
    unsigned int length)
{
    unsigned int index;

    for (index = 0U; index < length; index++) {
        unsigned int bit;

        crc ^= data[index];
        for (bit = 0U; bit < 8U; bit++) {
            crc = (crc >> 1U) ^ ((crc & 1U) != 0U ? 0xEDB88320U : 0U);
        }
    }
    return crc;
}

static unsigned int record_crc(const unsigned char *sector)
{
    unsigned int crc = 0xFFFFFFFFU;

    crc = crc32_update(crc, sector, 16U);
    crc = crc32_update(crc, sector + JOURNAL_HEADER_SIZE, JOURNAL_PAYLOAD_SIZE);
    return ~crc;
}

static unsigned int superblock_crc(const unsigned char *sector)
{
    unsigned int crc = 0xFFFFFFFFU;

    crc = crc32_update(crc, sector, 12U);
    return ~crc;
}

static unsigned int bounded_length(const char *value, unsigned int limit)
{
    unsigned int length = 0U;

    if (value == 0) {
        return limit;
    }
    while (length < limit && value[length] != '\0') {
        length++;
    }
    return length;
}

static int valid_name(const unsigned char *name, unsigned int length)
{
    unsigned int index;

    if (name == 0 || length == 0U || length >= JOURNAL_NAME_MAX) {
        return 0;
    }
    for (index = 0U; index < length; index++) {
        if (name[index] < '!' || name[index] == '/' || name[index] == '\\') {
            return 0;
        }
    }
    return 1;
}

static int journal_encode_internal(unsigned char *sector, unsigned int operation,
    unsigned int sequence, const char *name, const void *content,
    unsigned int content_length, int committed)
{
    unsigned int name_length;
    unsigned int index;

    if (sector == 0 || sequence == 0U ||
        (operation != JOURNAL_OPERATION_WRITE && operation != JOURNAL_OPERATION_REMOVE) ||
        content_length > JOURNAL_CONTENT_MAX ||
        (content_length != 0U && content == 0)) {
        return 0;
    }
    name_length = bounded_length(name, JOURNAL_NAME_MAX);
    if (!valid_name((const unsigned char *)name, name_length) ||
        (operation == JOURNAL_OPERATION_REMOVE && content_length != 0U)) {
        return 0;
    }
    for (index = 0U; index < JOURNAL_SECTOR_SIZE; index++) {
        sector[index] = 0U;
    }
    write_u32(sector, 0U, JOURNAL_MAGIC);
    write_u16(sector, 4U, JOURNAL_VERSION);
    write_u16(sector, 6U, operation);
    write_u32(sector, 8U, sequence);
    write_u16(sector, 12U, name_length);
    write_u16(sector, 14U, content_length);
    for (index = 0U; index < name_length; index++) {
        sector[JOURNAL_HEADER_SIZE + index] = (unsigned char)name[index];
    }
    for (index = 0U; index < content_length; index++) {
        sector[JOURNAL_HEADER_SIZE + JOURNAL_NAME_MAX + index] =
            ((const unsigned char *)content)[index];
    }
    write_u32(sector, 16U, record_crc(sector));
    if (committed) {
        write_u32(sector, JOURNAL_COMMIT_OFFSET, JOURNAL_COMMIT);
    }
    return 1;
}

int journal_encode(unsigned char *sector, unsigned int operation,
    unsigned int sequence, const char *name, const void *content,
    unsigned int content_length)
{
    return journal_encode_internal(sector, operation, sequence, name, content,
        content_length, 1);
}

int journal_decode(const unsigned char *sector, struct journal_entry *entry)
{
    unsigned int name_length;
    unsigned int content_length;
    unsigned int index;

    if (sector == 0 || entry == 0 || read_u32(sector, 0U) != JOURNAL_MAGIC ||
        read_u16(sector, 4U) != JOURNAL_VERSION || read_u32(sector, 8U) == 0U ||
        read_u32(sector, JOURNAL_COMMIT_OFFSET) != JOURNAL_COMMIT) {
        return 0;
    }
    if (read_u16(sector, 6U) != JOURNAL_OPERATION_WRITE &&
        read_u16(sector, 6U) != JOURNAL_OPERATION_REMOVE) {
        return 0;
    }
    name_length = read_u16(sector, 12U);
    content_length = read_u16(sector, 14U);
    if (!valid_name(sector + JOURNAL_HEADER_SIZE, name_length) ||
        content_length > JOURNAL_CONTENT_MAX ||
        (read_u16(sector, 6U) == JOURNAL_OPERATION_REMOVE && content_length != 0U) ||
        read_u32(sector, 16U) != record_crc(sector)) {
        return 0;
    }
    if (sector[JOURNAL_HEADER_SIZE + name_length] != 0U) {
        return 0;
    }
    for (index = name_length + 1U; index < JOURNAL_NAME_MAX; index++) {
        if (sector[JOURNAL_HEADER_SIZE + index] != 0U) {
            return 0;
        }
    }
    for (index = content_length; index < JOURNAL_CONTENT_MAX; index++) {
        if (sector[JOURNAL_HEADER_SIZE + JOURNAL_NAME_MAX + index] != 0U) {
            return 0;
        }
    }
    for (index = JOURNAL_HEADER_SIZE + JOURNAL_PAYLOAD_SIZE;
         index < JOURNAL_COMMIT_OFFSET; index++) {
        if (sector[index] != 0U) {
            return 0;
        }
    }
    entry->operation = read_u16(sector, 6U);
    entry->sequence = read_u32(sector, 8U);
    entry->name_length = name_length;
    entry->content_length = content_length;
    for (index = 0U; index < JOURNAL_NAME_MAX; index++) {
        entry->name[index] = (char)sector[JOURNAL_HEADER_SIZE + index];
    }
    for (index = 0U; index < JOURNAL_CONTENT_MAX; index++) {
        entry->content[index] = sector[JOURNAL_HEADER_SIZE + JOURNAL_NAME_MAX + index];
    }
    return 1;
}

int journal_superblock_encode(unsigned char *sector, unsigned int data_sectors)
{
    unsigned int index;

    if (sector == 0 || data_sectors == 0U || data_sectors > JOURNAL_MAX_DATA_SECTORS) {
        return 0;
    }
    for (index = 0U; index < JOURNAL_SECTOR_SIZE; index++) {
        sector[index] = 0U;
    }
    write_u32(sector, 0U, JOURNAL_SUPERBLOCK_MAGIC);
    write_u16(sector, 4U, JOURNAL_SUPERBLOCK_VERSION);
    write_u16(sector, 6U, data_sectors);
    write_u32(sector, 8U, 0U);
    write_u32(sector, 12U, superblock_crc(sector));
    write_u32(sector, JOURNAL_COMMIT_OFFSET, JOURNAL_COMMIT);
    return 1;
}

int journal_superblock_decode(const unsigned char *sector, unsigned int *data_sectors)
{
    unsigned int index;
    unsigned int count;

    if (sector == 0 || read_u32(sector, 0U) != JOURNAL_SUPERBLOCK_MAGIC ||
        read_u16(sector, 4U) != JOURNAL_SUPERBLOCK_VERSION ||
        read_u32(sector, JOURNAL_COMMIT_OFFSET) != JOURNAL_COMMIT) {
        return 0;
    }
    count = read_u16(sector, 6U);
    if (count == 0U || count > JOURNAL_MAX_DATA_SECTORS ||
        read_u32(sector, 8U) != 0U || read_u32(sector, 12U) != superblock_crc(sector)) {
        return 0;
    }
    for (index = 16U; index < JOURNAL_COMMIT_OFFSET; index++) {
        if (sector[index] != 0U) {
            return 0;
        }
    }
    if (data_sectors != 0) {
        *data_sectors = count;
    }
    return 1;
}

static int sector_is_empty(const unsigned char *sector)
{
    unsigned int index;

    for (index = 0U; index < JOURNAL_SECTOR_SIZE; index++) {
        if (sector[index] != 0U) {
            return 0;
        }
    }
    return 1;
}

static int sequence_is_newer(unsigned int sequence, unsigned int previous)
{
    return sequence != 0U && sequence > previous;
}

static int region_fits(unsigned int start_lba, unsigned int sector_count)
{
    /* The callbacks receive absolute 32-bit LBAs. Reject a region whose
       final sector would wrap before doing any I/O. */
    return sector_count != 0U && start_lba <= 0xFFFFFFFFU - (sector_count - 1U);
}

static int scan_log(journal_read_fn read, unsigned int start_lba,
    unsigned int data_sectors, unsigned int *record_count,
    unsigned int *last_sequence)
{
    unsigned char sector[JOURNAL_SECTOR_SIZE];
    struct journal_entry entry;
    unsigned int previous_sequence = 0U;
    unsigned int count = 0U;
    unsigned int index;

    for (index = 0U; index < data_sectors; index++) {
        if (!read(start_lba + 1U + index, 1U, sector)) {
            return 0;
        }
        if (sector_is_empty(sector)) {
            break;
        }
        if (!journal_decode(sector, &entry) ||
            !sequence_is_newer(entry.sequence, previous_sequence)) {
            return 0;
        }
        previous_sequence = entry.sequence;
        count++;
    }
    for (; index < data_sectors; index++) {
        if (!read(start_lba + 1U + index, 1U, sector) || !sector_is_empty(sector)) {
            return 0;
        }
    }
    if (record_count != 0) {
        *record_count = count;
    }
    if (last_sequence != 0) {
        *last_sequence = previous_sequence;
    }
    return 1;
}

int journal_replay(journal_read_fn read, unsigned int start_lba,
    unsigned int sector_count, journal_apply_fn apply, unsigned int *applied)
{
    unsigned char sector[JOURNAL_SECTOR_SIZE];
    struct journal_entry entry;
    unsigned int data_sectors;
    unsigned int record_count = 0U;
    unsigned int index;

    if (applied != 0) {
        *applied = 0U;
    }
    if (read == 0 || apply == 0 || sector_count < 2U ||
        sector_count > JOURNAL_MAX_DATA_SECTORS + 1U ||
        !region_fits(start_lba, sector_count) ||
        !read(start_lba, 1U, sector) || !journal_superblock_decode(sector, &data_sectors) ||
        data_sectors + 1U > sector_count) {
        return 0;
    }
    /* First pass: validate the complete contiguous log without invoking the
       consumer. A non-empty invalid sector makes the entire replay fail closed. */
    if (!scan_log(read, start_lba, data_sectors, &record_count, 0)) {
        return 0;
    }
    /* Second pass: only now mutate the consumer state. */
    for (index = 0U; index < record_count; index++) {
        if (!read(start_lba + 1U + index, 1U, sector) ||
            !journal_decode(sector, &entry) || !apply(&entry)) {
            return 0;
        }
    }
    if (applied != 0) {
        *applied = record_count;
    }
    return 1;
}

int journal_append(journal_read_fn read, journal_write_fn write,
    unsigned int start_lba, unsigned int sector_count, unsigned int operation,
    unsigned int sequence, const char *name, const void *content,
    unsigned int content_length)
{
    unsigned char superblock[JOURNAL_SECTOR_SIZE];
    unsigned char sector[JOURNAL_SECTOR_SIZE];
    unsigned int data_sectors;
    unsigned int record_count;
    unsigned int last_sequence;
    unsigned int target_lba;
    struct journal_entry entry;

    if (read == 0 || write == 0 || sector_count < 2U ||
        sector_count > JOURNAL_MAX_DATA_SECTORS + 1U ||
        !region_fits(start_lba, sector_count) ||
        !read(start_lba, 1U, superblock) ||
        !journal_superblock_decode(superblock, &data_sectors) ||
        data_sectors + 1U > sector_count ||
        !scan_log(read, start_lba, data_sectors, &record_count, &last_sequence) ||
        record_count >= data_sectors || !sequence_is_newer(sequence, last_sequence) ||
        !journal_encode_internal(sector, operation, sequence, name, content,
            content_length, 0)) {
        return 0;
    }
    target_lba = start_lba + 1U + record_count;
    /* Two-phase sector publication: a prepared payload is never replayable;
       only the second write exposes the terminal commit marker. */
    if (!write(target_lba, 1U, sector)) {
        return 0;
    }
    write_u32(sector, JOURNAL_COMMIT_OFFSET, JOURNAL_COMMIT);
    if (!write(target_lba, 1U, sector) || !read(target_lba, 1U, superblock) ||
        !journal_decode(superblock, &entry)) {
        return 0;
    }
    return 1;
}

static int bytes_equal(const unsigned char *left, const unsigned char *right,
    unsigned int length)
{
    unsigned int index;

    for (index = 0U; index < length; index++) {
        if (left[index] != right[index]) {
            return 0;
        }
    }
    return 1;
}

int journal_self_test(void)
{
    unsigned char sector[JOURNAL_SECTOR_SIZE];
    unsigned char original[JOURNAL_SECTOR_SIZE];
    unsigned char content[] = {'s', 'a', 'f', 'e'};
    struct journal_entry entry;
    unsigned int index;

    if (!journal_encode(sector, JOURNAL_OPERATION_WRITE, 1U, "NOTE", content,
        sizeof(content)) || !journal_decode(sector, &entry) ||
        entry.operation != JOURNAL_OPERATION_WRITE || entry.sequence != 1U ||
        entry.name_length != 4U || entry.content_length != sizeof(content) ||
        !bytes_equal((const unsigned char *)entry.name, (const unsigned char *)"NOTE", 4U) ||
        !bytes_equal(entry.content, content, sizeof(content))) {
        return 0;
    }
    for (index = 0U; index < JOURNAL_SECTOR_SIZE; index++) {
        original[index] = sector[index];
    }
    sector[0] ^= 0x01U;
    if (journal_decode(sector, &entry)) {
        return 0;
    }
    for (index = 0U; index < JOURNAL_SECTOR_SIZE; index++) {
        sector[index] = original[index];
    }
    sector[JOURNAL_COMMIT_OFFSET] ^= 0x01U;
    if (journal_decode(sector, &entry)) {
        return 0;
    }
    for (index = 0U; index < JOURNAL_SECTOR_SIZE; index++) {
        sector[index] = original[index];
    }
    sector[JOURNAL_HEADER_SIZE] = '/';
    if (journal_decode(sector, &entry)) {
        return 0;
    }
    for (index = 0U; index < JOURNAL_SECTOR_SIZE; index++) {
        sector[index] = original[index];
    }
    if (journal_encode(sector, JOURNAL_OPERATION_REMOVE, 2U, "NOTE", 0, 0U) == 0 ||
        !journal_decode(sector, &entry) || entry.operation != JOURNAL_OPERATION_REMOVE ||
        entry.content_length != 0U) {
        return 0;
    }
    for (index = 0U; index < JOURNAL_SECTOR_SIZE; index++) {
        sector[index] = 0xFFU;
    }
    return !journal_decode(sector, &entry);
}
