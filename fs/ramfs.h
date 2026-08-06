#ifndef BEEROS_RAMFS_H
#define BEEROS_RAMFS_H

unsigned int ramfs_file_count(void);
const char *ramfs_file_name(unsigned int index);
const char *ramfs_file_contents(const char *name);

#endif
