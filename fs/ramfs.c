#include "ramfs.h"
#include "journal.h"

struct ramfs_file {
    char name[RAMFS_NAME_MAX];
    char contents[RAMFS_CONTENT_MAX];
};

static struct ramfs_file files[RAMFS_MAX_FILES];
static unsigned int file_count;

static int string_equals(const char *left, const char *right)
{
    if (left == 0 || right == 0) {
        return 0;
    }
    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return 0;
        }
        left++;
        right++;
    }
    return *left == *right;
}

static unsigned int string_length(const char *value, unsigned int limit)
{
    unsigned int length = 0;

    if (value == 0) {
        return limit;
    }
    while (length < limit && value[length] != '\0') {
        length++;
    }
    return length;
}

static int valid_name(const char *name)
{
    unsigned int index;
    unsigned int length = string_length(name, RAMFS_NAME_MAX);

    if (length == 0U || length >= RAMFS_NAME_MAX) {
        return 0;
    }
    for (index = 0; index < length; index++) {
        unsigned char character = (unsigned char)name[index];
        if (character < '!' || character == '/' || character == '\\') {
            return 0;
        }
    }
    return 1;
}

static void copy_string(char *destination, const char *source, unsigned int capacity)
{
    unsigned int index = 0;

    while (index + 1U < capacity && source[index] != '\0') {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
}

static void add_file(const char *name, const char *contents)
{
    if (file_count == RAMFS_MAX_FILES) {
        return;
    }
    copy_string(files[file_count].name, name, RAMFS_NAME_MAX);
    copy_string(files[file_count].contents, contents, RAMFS_CONTENT_MAX);
    file_count++;
}

void ramfs_init(void)
{
    file_count = 0;
    add_file("README", "EfesOS RAM filesystem\n");
    add_file("MOTD", "Build, learn, and drink responsibly.\n");
    add_file("EFES", "EfesOS RAM filesystem entry.\n");
}

unsigned int ramfs_file_count(void)
{
    return file_count;
}

const char *ramfs_file_name(unsigned int index)
{
    if (index >= file_count) {
        return "";
    }
    return files[index].name;
}

const char *ramfs_file_contents(const char *name)
{
    unsigned int index;

    if (!valid_name(name)) {
        return 0;
    }
    for (index = 0; index < file_count; index++) {
        if (string_equals(name, files[index].name)) {
            return files[index].contents;
        }
    }
    return 0;
}

int ramfs_write_file(const char *name, const char *contents)
{
    unsigned int index;
    unsigned int length;

    if (!valid_name(name) || contents == 0) {
        return 0;
    }
    length = string_length(contents, RAMFS_CONTENT_MAX);
    if (length >= RAMFS_CONTENT_MAX) {
        return 0;
    }
    for (index = 0; index < file_count; index++) {
        if (string_equals(name, files[index].name)) {
            copy_string(files[index].contents, contents, RAMFS_CONTENT_MAX);
            return 1;
        }
    }
    if (file_count == RAMFS_MAX_FILES) {
        return 0;
    }
    add_file(name, contents);
    return 1;
}

int ramfs_remove_file(const char *name)
{
    unsigned int index;

    if (!valid_name(name)) {
        return 0;
    }
    for (index = 0; index < file_count; index++) {
        unsigned int move;
        if (!string_equals(name, files[index].name)) {
            continue;
        }
        for (move = index + 1U; move < file_count; move++) {
            copy_string(files[move - 1U].name, files[move].name, RAMFS_NAME_MAX);
            copy_string(files[move - 1U].contents, files[move].contents, RAMFS_CONTENT_MAX);
        }
        file_count--;
        return 1;
    }
    return 0;
}

int ramfs_apply_journal_entry(const struct journal_entry *entry)
{
    char content[RAMFS_CONTENT_MAX];
    unsigned int index;

    if (entry == 0 || entry->name_length == 0U ||
        entry->name_length >= RAMFS_NAME_MAX ||
        entry->content_length >= RAMFS_CONTENT_MAX) {
        return 0;
    }
    if (entry->operation == JOURNAL_OPERATION_REMOVE) {
        return entry->content_length == 0U && ramfs_remove_file(entry->name);
    }
    if (entry->operation != JOURNAL_OPERATION_WRITE) {
        return 0;
    }
    for (index = 0U; index < entry->content_length; index++) {
        content[index] = (char)entry->content[index];
    }
    content[entry->content_length] = '\0';
    return ramfs_write_file(entry->name, content);
}

int ramfs_self_test(void)
{
    const char *contents;

    if (!ramfs_write_file("TEST", "safe write\n")) {
        return 0;
    }
    contents = ramfs_file_contents("TEST");
    if (contents == 0 || !string_equals(contents, "safe write\n") ||
        ramfs_write_file("bad/name", "rejected") ||
        ramfs_write_file("TOO_LONG_NAME_THAT_EXCEEDS_THE_LIMIT", "rejected") ||
        ramfs_file_contents("TOO_LONG_NAME_THAT_EXCEEDS_THE_LIMIT") != 0 ||
        !ramfs_remove_file("TEST") || ramfs_file_contents("TEST") != 0) {
        return 0;
    }
    return 1;
}
