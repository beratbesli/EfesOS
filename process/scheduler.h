#ifndef EFESOS_SCHEDULER_H
#define EFESOS_SCHEDULER_H

struct interrupt_frame;

#define SCHEDULER_TASK_RUNNABLE 0U
#define SCHEDULER_TASK_TERMINATED 1U
#define SCHEDULER_TASK_BLOCKED 2U

typedef void (*scheduler_task_t)(void);
typedef unsigned int scheduler_counter_t;

void scheduler_init(void);
int scheduler_add_task(const char *name, scheduler_task_t task);
int scheduler_add_user_task(const char *name, unsigned int entry, unsigned int user_stack_top);
int scheduler_add_user_task_in_space(const char *name, unsigned int entry,
    unsigned int user_stack_top, unsigned int address_space);
int scheduler_set_priority(unsigned int index, unsigned int priority);
int scheduler_block_task(unsigned int index);
int scheduler_wake_task(unsigned int index);
int scheduler_wake_task_id(unsigned int task_id);
unsigned int scheduler_wake_user_tasks(void);
int scheduler_task_id_is_active_user(unsigned int task_id);
int scheduler_block_current(void);
unsigned int scheduler_blocked_count(void);
void scheduler_start(void);
struct interrupt_frame *scheduler_on_timer(struct interrupt_frame *frame);
struct interrupt_frame *scheduler_on_yield(struct interrupt_frame *frame);
struct interrupt_frame *scheduler_on_user_fault(struct interrupt_frame *frame);
struct interrupt_frame *scheduler_on_user_exit(struct interrupt_frame *frame);
unsigned int scheduler_task_count(void);
const char *scheduler_task_name(unsigned int index);
scheduler_counter_t scheduler_task_runs(unsigned int index);
unsigned int scheduler_stack_reap_count(void);
unsigned int scheduler_current_task_index(void);
unsigned int scheduler_last_added_task(void);
unsigned int scheduler_current_task_id(void);
unsigned int scheduler_task_id(unsigned int index);

#endif
