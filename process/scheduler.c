#include "scheduler.h"

#define SCHEDULER_MAX_TASKS 8

struct scheduler_task {
    const char *name;
    scheduler_task_t entry;
    scheduler_counter_t runs;
};

static struct scheduler_task tasks[SCHEDULER_MAX_TASKS];
static unsigned int task_count;
static unsigned int current_task;
static unsigned char scheduler_started;

void scheduler_init(void)
{
    task_count = 0;
    current_task = 0;
    scheduler_started = 0;
}

int scheduler_add_task(const char *name, scheduler_task_t task)
{
    if (name == 0 || task == 0 || task_count == SCHEDULER_MAX_TASKS) {
        return 0;
    }

    tasks[task_count].name = name;
    tasks[task_count].entry = task;
    tasks[task_count].runs = 0;
    task_count++;
    return 1;
}

void scheduler_start(void)
{
    scheduler_started = 1;
}

void scheduler_tick(void)
{
    if (scheduler_started == 0 || task_count == 0) {
        return;
    }

    tasks[current_task].entry();
    tasks[current_task].runs++;
    current_task++;
    if (current_task == task_count) {
        current_task = 0;
    }
}

unsigned int scheduler_task_count(void)
{
    return task_count;
}

const char *scheduler_task_name(unsigned int index)
{
    if (index >= task_count) {
        return "";
    }

    return tasks[index].name;
}

scheduler_counter_t scheduler_task_runs(unsigned int index)
{
    if (index >= task_count) {
        return 0;
    }

    return tasks[index].runs;
}
