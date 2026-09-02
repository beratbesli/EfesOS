#ifndef EFESOS_JOURNAL_H
#define EFESOS_JOURNAL_H

#define JOURNAL_SECTOR_SIZE 512U
#define JOURNAL_NAME_MAX 32U
#define JOURNAL_CONTENT_MAX 256U
#define JOURNAL_MAX_DATA_SECTORS 1024U
#define JOURNAL_OPERATION_WRITE 1U
#define JOURNAL_OPERATION_REMOVE 2U

struct journal_entry {
    unsigned int operation;
    unsigned int sequence;
    unsigned int name_length;
    unsigned int content_length;
    char name[JOURNAL_NAME_MAX];
    unsigned char content[JOURNAL_CONTENT_MAX];
};

typedef int (*journal_read_fn)(unsigned int lba, unsigned char count, void *buffer);
typedef int (*journal_write_fn)(unsigned int lba, unsigned char count, const void *buffer);
typedef int (*journal_apply_fn)(const struct journal_entry *entry);

int journal_encode(unsigned char *sector, unsigned int operation,
    unsigned int sequence, const char *name, const void *content,
    unsigned int content_length);
int journal_decode(const unsigned char *sector, struct journal_entry *entry);
int journal_superblock_encode(unsigned char *sector, unsigned int data_sectors);
int journal_superblock_decode(const unsigned char *sector, unsigned int *data_sectors);
int journal_replay(journal_read_fn read, unsigned int start_lba,
    unsigned int sector_count, journal_apply_fn apply, unsigned int *applied);
int journal_append(journal_read_fn read, journal_write_fn write,
    unsigned int start_lba, unsigned int sector_count, unsigned int operation,
    unsigned int sequence, const char *name, const void *content,
    unsigned int content_length);
int journal_self_test(void);

#endif
