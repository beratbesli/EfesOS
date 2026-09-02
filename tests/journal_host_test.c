#include <stdio.h>
#include <string.h>

#include "journal.h"

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
    puts("Journal host self-test passed.");
    return 0;
}
