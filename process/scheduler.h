#ifndef BEEROS_SCHEDULER_H
#define BEEROS_SCHEDULER_H

typedef void (*scheduler_task_t)(void);

void scheduler_init(void);
int scheduler_add_task(scheduler_task_t task);
void scheduler_run(void);

#endif
