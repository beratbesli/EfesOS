#ifndef EFESOS_PERSISTENT_H
#define EFESOS_PERSISTENT_H

int persistent_ramfs_init(void);
int persistent_ramfs_format(void);
int persistent_ramfs_is_enabled(void);
unsigned int persistent_ramfs_replay_count(void);
int persistent_ramfs_write_file(const char *name, const char *contents);
int persistent_ramfs_remove_file(const char *name);

#endif
