#ifndef EFESOS_USER_PROCESS_H
#define EFESOS_USER_PROCESS_H

int user_process_init(void);
void user_process_reap(void);
unsigned int user_process_reap_count(void);
unsigned int user_process_address_space(void);

#endif
