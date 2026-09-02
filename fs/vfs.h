#ifndef EFESOS_VFS_H
#define EFESOS_VFS_H

void vfs_init(void);
int vfs_is_mounted(void);
unsigned int vfs_file_count(void);
int vfs_file_name(unsigned int index, char *name, unsigned int capacity);
int vfs_read_file(const char *name, void *buffer, unsigned int capacity, unsigned int *size);
int vfs_journal_region_available(unsigned int start_lba, unsigned int sector_count);

#endif
