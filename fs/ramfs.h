#ifndef EFESOS_RAMFS_H
#define EFESOS_RAMFS_H

#define RAMFS_MAX_FILES 16U
#define RAMFS_NAME_MAX 32U
#define RAMFS_CONTENT_MAX 256U

void ramfs_init(void);
unsigned int ramfs_file_count(void);
const char *ramfs_file_name(unsigned int index);
const char *ramfs_file_contents(const char *name);
int ramfs_write_file(const char *name, const char *contents);
int ramfs_remove_file(const char *name);
int ramfs_self_test(void);

#endif
