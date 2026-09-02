#include <stdio.h>

#include "journal.h"
#include "ramfs.h"

int main(void)
{
    char unterminated[RAMFS_NAME_MAX];
    unsigned int index;

    ramfs_init();
    if (ramfs_file_count() != 3U || ramfs_file_contents("README") == 0 ||
        ramfs_file_contents(0) != 0 || ramfs_file_contents("bad/name") != 0 ||
        ramfs_file_contents("TOO_LONG_NAME_THAT_EXCEEDS_THE_LIMIT") != 0) {
        return 1;
    }
    for (index = 0U; index < sizeof(unterminated); index++) {
        unterminated[index] = 'X';
    }
    if (ramfs_file_contents(unterminated) != 0) {
        return 1;
    }
    if (!ramfs_write_file("HOST", "host test\n") ||
        ramfs_file_contents("HOST") == 0 ||
        !ramfs_remove_file("HOST") || ramfs_file_contents("HOST") != 0 ||
        ramfs_write_file("bad/name", "rejected") ||
        ramfs_write_file("HOST", 0)) {
        return 1;
    }
    {
        struct journal_entry entry = {0};
        entry.operation = JOURNAL_OPERATION_WRITE;
        entry.sequence = 1U;
        entry.name_length = 7U;
        entry.content_length = 5U;
        entry.name[0] = 'P';
        entry.name[1] = 'R';
        entry.name[2] = 'E';
        entry.name[3] = 'F';
        entry.name[4] = 'S';
        entry.name[5] = 'E';
        entry.name[6] = 'T';
        entry.content[0] = 'r';
        entry.content[1] = 'e';
        entry.content[2] = 'p';
        entry.content[3] = 'l';
        entry.content[4] = 'y';
        if (!ramfs_apply_journal_entry(&entry) ||
            ramfs_file_contents("PREFSET") == 0 ||
            !ramfs_remove_file("PREFSET")) {
            return 1;
        }
        entry.operation = JOURNAL_OPERATION_REMOVE;
        entry.content_length = 0U;
        if (!ramfs_apply_journal_entry(&entry)) {
            return 1;
        }
    }
    puts("RAMFS host self-test passed.");
    return 0;
}
