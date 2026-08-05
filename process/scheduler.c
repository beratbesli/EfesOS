#include "scheduler.h"

#define SCHEDULER_MAX_TASKS 8

static scheduler_task_t tasks[SCHEDULER_MAX_TASKS];
static unsigned int task_count;
static unsigned int current_task;

void scheduler_init(void)
{
    task_count = 0;
    current_task = 0;
}

int scheduler_add_task(scheduler_task_t task)
{
    if (task == 0 || task_count == SCHEDULER_MAX_TASKS) {
        return 0;
    }

    tasks[task_count] = task;
    task_count++;
    return 1;
}

void scheduler_run(void)
{
    for (current_task = 0; current_task < task_count; current_task++) {
        tasks[current_task]();
    }
}
