#include "ramfs.h"

struct ramfs_file {
    const char *name;
    const char *contents;
};

static const struct ramfs_file files[] = {
    { "README", "AyranOS RAM filesystem\n" },
    { "MOTD", "Build, learn, and drink responsibly.\n" },
    { "AYRAN", "Cold ayran, warm kernel.\n" }
};

static int string_equals(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return 0;
        }
        left++;
        right++;
    }

    return *left == *right;
}

unsigned int ramfs_file_count(void)
{
    return sizeof(files) / sizeof(files[0]);
}

const char *ramfs_file_name(unsigned int index)
{
    if (index >= ramfs_file_count()) {
        return "";
    }

    return files[index].name;
}

const char *ramfs_file_contents(const char *name)
{
    unsigned int index;

    for (index = 0; index < ramfs_file_count(); index++) {
        if (string_equals(name, files[index].name)) {
            return files[index].contents;
        }
    }

    return 0;
}
