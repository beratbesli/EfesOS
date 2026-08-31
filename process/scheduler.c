#include "scheduler.h"
#include "heap.h"
#include "idt.h"
#include "paging.h"
#include "panic.h"
#include "pmm.h"
#include "user_process.h"

#define SCHEDULER_MAX_TASKS 8U
#define SCHEDULER_STACK_PAGES 4U
#define SCHEDULER_STACK_STRIDE (PAGE_SIZE * (SCHEDULER_STACK_PAGES + 1U))
#define SCHEDULER_STACK_BASE 0xC0000000U
#define TASK_RUNNABLE 0U
#define TASK_TERMINATED 1U
#define TASK_BLOCKED 2U
#define TASK_KERNEL 0U
#define TASK_USER 1U
#define SCHEDULER_DEFAULT_PRIORITY 1U
#define SCHEDULER_MAX_PRIORITY 8U

struct scheduler_task {
    const char *name;
    scheduler_task_t entry;
    struct interrupt_frame *frame;
    unsigned int stack_base;
    unsigned int stack_frames[SCHEDULER_STACK_PAGES];
    unsigned int user_entry;
    unsigned int user_stack_top;
    unsigned int address_space;
    unsigned int mode;
    unsigned int priority;
    unsigned int ticks_left;
    scheduler_counter_t switches;
    unsigned int state;
};

static struct scheduler_task tasks[SCHEDULER_MAX_TASKS];
static unsigned int task_count;
static unsigned int current_task;
static unsigned char scheduler_started;
static unsigned int pending_reap;
static unsigned int stack_reap_count;

static void scheduler_task_trampoline(void);

static void reap_task_stack(struct scheduler_task *task)
{
    unsigned int index;

    if (task == 0 || task->stack_base == 0U) {
        return;
    }
    for (index = 0; index < SCHEDULER_STACK_PAGES; index++) {
        unsigned int physical = paging_unmap_page(task->stack_base + PAGE_SIZE +
            (index * PAGE_SIZE));
        if (physical != 0U) {
            pmm_free_block(physical);
        }
        task->stack_frames[index] = 0U;
    }
    task->stack_base = 0U;
    task->frame = 0;
    stack_reap_count++;
}

static void clear_task(struct scheduler_task *task)
{
    unsigned int index;

    for (index = 0; index < SCHEDULER_STACK_PAGES; index++) {
        task->stack_frames[index] = 0;
    }
    task->name = 0;
    task->entry = 0;
    task->frame = 0;
    task->stack_base = 0;
    task->user_entry = 0;
    task->user_stack_top = 0;
    task->address_space = 0;
    task->mode = TASK_KERNEL;
    task->priority = SCHEDULER_DEFAULT_PRIORITY;
    task->ticks_left = SCHEDULER_DEFAULT_PRIORITY;
    task->switches = 0;
    task->state = TASK_TERMINATED;
}

static int allocate_task_stack(struct scheduler_task *task, unsigned int slot)
{
    unsigned int index;
    unsigned int base = SCHEDULER_STACK_BASE + (slot * SCHEDULER_STACK_STRIDE);

    task->stack_base = base;
    for (index = 0; index < SCHEDULER_STACK_PAGES; index++) {
        unsigned int physical = pmm_alloc_block();
        unsigned int virtual_address = base + PAGE_SIZE + (index * PAGE_SIZE);

        if (physical == 0U || !paging_map_page(virtual_address, physical, PAGE_FLAG_WRITABLE)) {
            if (physical != 0U) {
                pmm_free_block(physical);
            }
            while (index != 0U) {
                index--;
                physical = paging_unmap_page(base + PAGE_SIZE + (index * PAGE_SIZE));
                if (physical != 0U) {
                    pmm_free_block(physical);
                }
            }
            task->stack_base = 0;
            return 0;
        }
        task->stack_frames[index] = physical;
    }

    task->frame = (struct interrupt_frame *)(base + SCHEDULER_STACK_STRIDE - sizeof(struct interrupt_frame) - 8U);
    task->frame->gs = task->mode == TASK_USER ? 0x23U : 0x10U;
    task->frame->fs = task->mode == TASK_USER ? 0x23U : 0x10U;
    task->frame->es = task->mode == TASK_USER ? 0x23U : 0x10U;
    task->frame->ds = task->mode == TASK_USER ? 0x23U : 0x10U;
    task->frame->edi = 0;
    task->frame->esi = 0;
    task->frame->ebp = 0;
    task->frame->esp_before_pushad = 0;
    task->frame->ebx = 0;
    task->frame->edx = 0;
    task->frame->ecx = 0;
    task->frame->eax = 0;
    task->frame->vector = 0;
    task->frame->error_code = 0;
    task->frame->eip = task->mode == TASK_USER ? task->user_entry : (unsigned int)scheduler_task_trampoline;
    task->frame->cs = task->mode == TASK_USER ? 0x1BU : 0x08U;
    task->frame->eflags = 0x202;
    if (task->mode == TASK_USER) {
        unsigned int *return_words = (unsigned int *)(task->frame + 1);
        return_words[0] = task->user_stack_top;
        return_words[1] = 0x23U;
    }
    return 1;
}

static unsigned int find_next_runnable(void)
{
    unsigned int offset;

    for (offset = 1; offset <= task_count; offset++) {
        unsigned int index = (current_task + offset) % task_count;
        if (tasks[index].state == TASK_RUNNABLE && tasks[index].frame != 0) {
            return index;
        }
    }
    return current_task;
}

static int has_other_runnable(void)
{
    unsigned int index;

    for (index = 0U; index < task_count; index++) {
        if (index != current_task && tasks[index].state == TASK_RUNNABLE &&
            tasks[index].frame != 0) {
            return 1;
        }
    }
    return 0;
}

static void save_user_frame(struct scheduler_task *task, const struct interrupt_frame *frame)
{
    unsigned int index;
    unsigned int *source_extra;
    unsigned int *destination_extra;

    if (task->frame == 0 || task->frame == frame) {
        return;
    }
    for (index = 0; index < sizeof(struct interrupt_frame) / sizeof(unsigned int); index++) {
        ((unsigned int *)task->frame)[index] = ((const unsigned int *)frame)[index];
    }
    source_extra = (unsigned int *)(frame + 1);
    destination_extra = (unsigned int *)(task->frame + 1);
    destination_extra[0] = source_extra[0];
    destination_extra[1] = source_extra[1];
}

static struct interrupt_frame *schedule_from_frame(struct interrupt_frame *frame, int force)
{
    unsigned int next;

    if (!scheduler_started || task_count == 0U || frame == 0) {
        return frame;
    }
    if (pending_reap < task_count && pending_reap != current_task) {
        reap_task_stack(&tasks[pending_reap]);
        pending_reap = SCHEDULER_MAX_TASKS;
    }
    if (tasks[current_task].mode == TASK_USER) {
        save_user_frame(&tasks[current_task], frame);
    } else {
        tasks[current_task].frame = frame;
    }
    if (force == 0 && tasks[current_task].state == TASK_RUNNABLE) {
        if (tasks[current_task].ticks_left > 1U) {
            tasks[current_task].ticks_left--;
            return frame;
        }
    }
    next = find_next_runnable();
    if (next == current_task) {
        return frame;
    }
    current_task = next;
    tasks[current_task].switches++;
    tasks[current_task].ticks_left = tasks[current_task].priority;
    if (!paging_switch_address_space(tasks[current_task].address_space)) {
        kernel_panic("Task address-space switch failed.");
    }
    return tasks[current_task].frame;
}

void scheduler_init(void)
{
    unsigned int index;

    for (index = 0; index < SCHEDULER_MAX_TASKS; index++) {
        clear_task(&tasks[index]);
    }
    task_count = 1;
    current_task = 0;
    scheduler_started = 0;
    pending_reap = SCHEDULER_MAX_TASKS;
    stack_reap_count = 0U;
    tasks[0].name = "kernel";
    tasks[0].state = TASK_RUNNABLE;
    tasks[0].address_space = paging_kernel_directory();
}

int scheduler_add_task(const char *name, scheduler_task_t task)
{
    struct scheduler_task *new_task;

    if (name == 0 || task == 0 || scheduler_started != 0 || task_count == SCHEDULER_MAX_TASKS) {
        return 0;
    }
    new_task = &tasks[task_count];
    clear_task(new_task);
    new_task->name = name;
    new_task->entry = task;
    new_task->address_space = paging_kernel_directory();
    new_task->state = TASK_RUNNABLE;
    if (!allocate_task_stack(new_task, task_count)) {
        clear_task(new_task);
        return 0;
    }
    task_count++;
    return 1;
}

int scheduler_set_priority(unsigned int index, unsigned int priority)
{
    if (index == 0U || index >= task_count || priority == 0U ||
        priority > SCHEDULER_MAX_PRIORITY || scheduler_started != 0) {
        return 0;
    }
    tasks[index].priority = priority;
    tasks[index].ticks_left = priority;
    return 1;
}

int scheduler_block_task(unsigned int index)
{
    if (index == 0U || index >= task_count || scheduler_started != 0 ||
        tasks[index].state != TASK_RUNNABLE || tasks[index].frame == 0) {
        return 0;
    }
    tasks[index].state = TASK_BLOCKED;
    return 1;
}

int scheduler_wake_task(unsigned int index)
{
    if (index == 0U || index >= task_count || tasks[index].state != TASK_BLOCKED ||
        tasks[index].frame == 0) {
        return 0;
    }
    tasks[index].state = TASK_RUNNABLE;
    tasks[index].ticks_left = tasks[index].priority;
    return 1;
}

int scheduler_block_current(void)
{
    if (!scheduler_started || current_task == 0U || current_task >= task_count ||
        tasks[current_task].state != TASK_RUNNABLE || !has_other_runnable()) {
        return 0;
    }
    tasks[current_task].state = TASK_BLOCKED;
    return 1;
}

int scheduler_add_user_task_in_space(const char *name, unsigned int entry,
    unsigned int user_stack_top, unsigned int address_space)
{
    struct scheduler_task *new_task;

    if (name == 0 || entry == 0U || user_stack_top == 0U || scheduler_started != 0 ||
        address_space == 0U || task_count == SCHEDULER_MAX_TASKS) {
        return 0;
    }
    new_task = &tasks[task_count];
    clear_task(new_task);
    new_task->name = name;
    new_task->user_entry = entry;
    new_task->user_stack_top = user_stack_top;
    new_task->address_space = address_space;
    new_task->mode = TASK_USER;
    new_task->state = TASK_RUNNABLE;
    if (!allocate_task_stack(new_task, task_count)) {
        clear_task(new_task);
        return 0;
    }
    task_count++;
    return 1;
}

int scheduler_add_user_task(const char *name, unsigned int entry, unsigned int user_stack_top)
{
    return scheduler_add_user_task_in_space(name, entry, user_stack_top,
        paging_current_directory());
}

void scheduler_start(void)
{
    /* The boot call stack is not a schedulable task. All runtime work uses a
       dedicated task stack, so saved interrupt frames remain stable. */
    tasks[0].state = TASK_TERMINATED;
    scheduler_started = 1;
}

struct interrupt_frame *scheduler_on_timer(struct interrupt_frame *frame)
{
    return schedule_from_frame(frame, 0);
}

struct interrupt_frame *scheduler_on_yield(struct interrupt_frame *frame)
{
    return schedule_from_frame(frame, 1);
}

struct interrupt_frame *scheduler_on_user_fault(struct interrupt_frame *frame)
{
    if (!scheduler_started || current_task >= task_count || tasks[current_task].mode != TASK_USER) {
        return frame;
    }
    tasks[current_task].state = TASK_TERMINATED;
    pending_reap = current_task;
    user_process_reap();
    return schedule_from_frame(frame, 1);
}

static void scheduler_task_trampoline(void)
{
    scheduler_task_t entry = tasks[current_task].entry;

    if (entry != 0) {
        entry();
    }
    tasks[current_task].state = TASK_TERMINATED;
    pending_reap = current_task;
    __asm__ volatile ("int $0x31" : : : "memory");
    for (;;) {
        __asm__ volatile ("hlt");
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
    return tasks[index].switches;
}

unsigned int scheduler_stack_reap_count(void)
{
    return stack_reap_count;
}

unsigned int scheduler_blocked_count(void)
{
    unsigned int index;
    unsigned int blocked = 0U;

    for (index = 0U; index < task_count; index++) {
        if (tasks[index].state == TASK_BLOCKED) {
            blocked++;
        }
    }
    return blocked;
}
