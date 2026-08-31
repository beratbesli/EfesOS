#ifndef EFESOS_SCHEDULER_H
#define EFESOS_SCHEDULER_H

struct interrupt_frame;

typedef void (*scheduler_task_t)(void);
typedef unsigned int scheduler_counter_t;

void scheduler_init(void);
int scheduler_add_task(const char *name, scheduler_task_t task);
int scheduler_add_user_task(const char *name, unsigned int entry, unsigned int user_stack_top);
void scheduler_start(void);
struct interrupt_frame *scheduler_on_timer(struct interrupt_frame *frame);
struct interrupt_frame *scheduler_on_yield(struct interrupt_frame *frame);
unsigned int scheduler_task_count(void);
const char *scheduler_task_name(unsigned int index);
scheduler_counter_t scheduler_task_runs(unsigned int index);

#endif
