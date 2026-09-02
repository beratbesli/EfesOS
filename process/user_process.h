#ifndef EFESOS_USER_PROCESS_H
#define EFESOS_USER_PROCESS_H

int user_process_init(void);
/* Kernel-only bounded ELF spawn; image must remain readable for the call. */
int user_process_spawn(const char *name, const void *image, unsigned int image_size);
void user_process_reap(void);
int user_process_reap_task(unsigned int task_index, unsigned int task_id);
unsigned int user_process_reap_count(void);
unsigned int user_process_address_space(void);
unsigned int user_process_active_count(void);
unsigned int user_process_address_space_at(unsigned int index);
unsigned int user_process_stack_address_at(unsigned int index);

#endif
