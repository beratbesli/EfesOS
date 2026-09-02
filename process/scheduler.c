#include "scheduler.h"
#include "heap.h"
#include "ipc.h"
#include "idt.h"
#include "paging.h"
#include "panic.h"
#include "pmm.h"
#include "user_process.h"

#define SCHEDULER_MAX_TASKS 8U
#define SCHEDULER_STACK_PAGES 4U
#define SCHEDULER_STACK_STRIDE (PAGE_SIZE * (SCHEDULER_STACK_PAGES + 1U))
#define SCHEDULER_STACK_BASE 0xC0000000U
#define TASK_RUNNABLE SCHEDULER_TASK_RUNNABLE
#define TASK_TERMINATED SCHEDULER_TASK_TERMINATED
#define TASK_BLOCKED SCHEDULER_TASK_BLOCKED
#define TASK_KERNEL 0U
#define TASK_USER 1U
#define SCHEDULER_USER_MIN_ADDRESS 0x00400000U
#define SCHEDULER_USER_ADDRESS_LIMIT 0xC0000000U
#define SCHEDULER_DEFAULT_PRIORITY 1U
#define SCHEDULER_MAX_PRIORITY 8U
#define SCHEDULER_MAX_GENERATION 0x00FFFFFFU
#define SCHEDULER_NAME_MAX 16U

struct scheduler_task {
    char name[SCHEDULER_NAME_MAX];
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
    unsigned int task_id;
};

static struct scheduler_task tasks[SCHEDULER_MAX_TASKS];
static unsigned int task_count;
static unsigned int current_task;
static unsigned char scheduler_started;
static unsigned int pending_reap_mask;
static unsigned int stack_reap_count;
static unsigned int last_added_task;
static unsigned int slot_generation[SCHEDULER_MAX_TASKS];

static void scheduler_task_trampoline(void);

static void clear_kernel_page(unsigned int address)
{
    unsigned int *page = (unsigned int *)address;
    unsigned int index;

    for (index = 0U; index < PAGE_SIZE / sizeof(unsigned int); index++) {
        page[index] = 0U;
    }
}

static int copy_task_name(char *destination, const char *source)
{
    unsigned int index;

    if (destination == 0 || source == 0) {
        return 0;
    }
    for (index = 0U; index + 1U < SCHEDULER_NAME_MAX && source[index] != '\0'; index++) {
        destination[index] = source[index];
    }
    if (source[index] != '\0') {
        return 0;
    }
    destination[index] = '\0';
    return 1;
}

static unsigned int scheduler_irq_save(void)
{
    unsigned int flags;

    __asm__ volatile ("pushfl\n\tpopl %0\n\tcli" : "=r"(flags) : : "memory");
    return flags;
}

static void scheduler_irq_restore(unsigned int flags)
{
    __asm__ volatile ("pushl %0\n\tpopfl" : : "r"(flags) : "memory");
}

static unsigned int find_task_slot(void)
{
    unsigned int index;

    for (index = 1U; index < task_count; index++) {
        if ((pending_reap_mask & (1U << index)) == 0U &&
            tasks[index].state == TASK_TERMINATED &&
            tasks[index].stack_base == 0U && tasks[index].frame == 0) {
            return index;
        }
    }
    if (task_count < SCHEDULER_MAX_TASKS) {
        return task_count;
    }
    return SCHEDULER_MAX_TASKS;
}

static void reap_task_stack(struct scheduler_task *task)
{
    unsigned int index;

    if (task == 0 || task->stack_base == 0U) {
        return;
    }
    if (paging_is_mapped(task->stack_base)) {
        kernel_panic("Kernel task stack guard page is mapped.");
    }
    for (index = 0; index < SCHEDULER_STACK_PAGES; index++) {
        unsigned int expected = task->stack_frames[index];
        unsigned int physical;

        if (expected == 0U) {
            kernel_panic("Kernel task stack ownership is incomplete.");
        }
        physical = paging_unmap_page(task->stack_base + PAGE_SIZE +
            (index * PAGE_SIZE));
        if (physical != expected) {
            kernel_panic("Kernel task stack mapping ownership mismatch.");
        }
        pmm_free_block(physical);
        task->stack_frames[index] = 0U;
    }
    task->stack_base = 0U;
    task->frame = 0;
    stack_reap_count++;
}

static void reap_pending_stacks(void)
{
    unsigned int index;

    for (index = 1U; index < task_count; index++) {
        if ((pending_reap_mask & (1U << index)) != 0U && index != current_task) {
            reap_task_stack(&tasks[index]);
            pending_reap_mask &= ~(1U << index);
        }
    }
}

static void clear_task(struct scheduler_task *task)
{
    unsigned int index;

    for (index = 0U; index < SCHEDULER_NAME_MAX; index++) {
        task->name[index] = '\0';
    }
    for (index = 0; index < SCHEDULER_STACK_PAGES; index++) {
        task->stack_frames[index] = 0;
    }
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
    task->task_id = 0U;
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
        clear_kernel_page(virtual_address);
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
    reap_pending_stacks();
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
    pending_reap_mask = 0U;
    stack_reap_count = 0U;
    last_added_task = SCHEDULER_MAX_TASKS;
    for (index = 0U; index < SCHEDULER_MAX_TASKS; index++) {
        slot_generation[index] = 0U;
    }
    copy_task_name(tasks[0].name, "kernel");
    tasks[0].state = TASK_RUNNABLE;
    tasks[0].address_space = paging_kernel_directory();
    tasks[0].task_id = 0U;
}

int scheduler_add_task(const char *name, scheduler_task_t task)
{
    struct scheduler_task *new_task;
    unsigned int slot;
    unsigned int flags;

    if (name == 0 || task == 0) {
        return 0;
    }
    flags = scheduler_irq_save();
    slot = find_task_slot();
    if (slot == SCHEDULER_MAX_TASKS) {
        scheduler_irq_restore(flags);
        return 0;
    }
    new_task = &tasks[slot];
    if (slot_generation[slot] >= SCHEDULER_MAX_GENERATION) {
        scheduler_irq_restore(flags);
        return 0;
    }
    clear_task(new_task);
    if (!copy_task_name(new_task->name, name)) {
        scheduler_irq_restore(flags);
        return 0;
    }
    new_task->entry = task;
    new_task->address_space = paging_kernel_directory();
    new_task->state = TASK_RUNNABLE;
    slot_generation[slot]++;
    if (slot_generation[slot] == 0U) {
        slot_generation[slot]++;
    }
    new_task->task_id = (slot_generation[slot] << 8U) | slot;
    if (!allocate_task_stack(new_task, slot)) {
        clear_task(new_task);
        scheduler_irq_restore(flags);
        return 0;
    }
    if (slot == task_count) {
        task_count++;
    }
    last_added_task = slot;
    scheduler_irq_restore(flags);
    return 1;
}

int scheduler_set_priority(unsigned int index, unsigned int priority)
{
    unsigned int flags;
    int result = 0;

    flags = scheduler_irq_save();
    if (index == 0U || index >= task_count || priority == 0U ||
        priority > SCHEDULER_MAX_PRIORITY || scheduler_started != 0) {
        scheduler_irq_restore(flags);
        return 0;
    }
    tasks[index].priority = priority;
    tasks[index].ticks_left = priority;
    result = 1;
    scheduler_irq_restore(flags);
    return result;
}

int scheduler_block_task(unsigned int index)
{
    unsigned int flags = scheduler_irq_save();

    if (index == 0U || index >= task_count || scheduler_started != 0 ||
        tasks[index].state != TASK_RUNNABLE || tasks[index].frame == 0) {
        scheduler_irq_restore(flags);
        return 0;
    }
    tasks[index].state = TASK_BLOCKED;
    scheduler_irq_restore(flags);
    return 1;
}

static int scheduler_wake_task_unlocked(unsigned int index)
{
    if (index == 0U || index >= task_count || tasks[index].state != TASK_BLOCKED ||
        tasks[index].frame == 0) {
        return 0;
    }
    tasks[index].state = TASK_RUNNABLE;
    tasks[index].ticks_left = tasks[index].priority;
    return 1;
}

int scheduler_wake_task(unsigned int index)
{
    unsigned int flags = scheduler_irq_save();
    int result = scheduler_wake_task_unlocked(index);

    scheduler_irq_restore(flags);
    return result;
}

int scheduler_wake_task_id(unsigned int task_id)
{
    unsigned int index;
    unsigned int flags;
    int result = 0;

    if (task_id == 0U) {
        return 0;
    }
    flags = scheduler_irq_save();
    for (index = 1U; index < task_count; index++) {
        if (tasks[index].task_id == task_id) {
            result = scheduler_wake_task_unlocked(index);
            break;
        }
    }
    scheduler_irq_restore(flags);
    return result;
}

unsigned int scheduler_wake_user_tasks(void)
{
    unsigned int index;
    unsigned int woken = 0U;
    unsigned int flags = scheduler_irq_save();

    for (index = 1U; index < task_count; index++) {
        if (tasks[index].mode == TASK_USER && scheduler_wake_task_unlocked(index)) {
            woken++;
        }
    }
    scheduler_irq_restore(flags);
    return woken;
}

int scheduler_task_id_is_active_user(unsigned int task_id)
{
    unsigned int index;
    unsigned int flags;
    int result = 0;

    if (task_id == 0U) {
        return 0;
    }
    flags = scheduler_irq_save();
    for (index = 1U; index < task_count; index++) {
        if (tasks[index].task_id == task_id && tasks[index].mode == TASK_USER &&
            tasks[index].state != TASK_TERMINATED && tasks[index].frame != 0) {
            result = 1;
            break;
        }
    }
    scheduler_irq_restore(flags);
    return result;
}

int scheduler_block_current(void)
{
    unsigned int flags = scheduler_irq_save();

    if (!scheduler_started || current_task == 0U || current_task >= task_count ||
        tasks[current_task].state != TASK_RUNNABLE || !has_other_runnable()) {
        scheduler_irq_restore(flags);
        return 0;
    }
    tasks[current_task].state = TASK_BLOCKED;
    scheduler_irq_restore(flags);
    return 1;
}

int scheduler_add_user_task_in_space(const char *name, unsigned int entry,
    unsigned int user_stack_top, unsigned int address_space)
{
    struct scheduler_task *new_task;
    unsigned int slot;
    unsigned int flags;

    if (name == 0 || entry < SCHEDULER_USER_MIN_ADDRESS ||
        entry >= SCHEDULER_USER_ADDRESS_LIMIT ||
        user_stack_top < SCHEDULER_USER_MIN_ADDRESS ||
        user_stack_top > SCHEDULER_USER_ADDRESS_LIMIT ||
        (user_stack_top & (PAGE_SIZE - 1U)) != 0U ||
        !paging_address_space_is_valid(address_space) ||
        address_space == paging_kernel_directory() ||
        paging_current_directory() != paging_kernel_directory()) {
        return 0;
    }
    flags = scheduler_irq_save();
    slot = find_task_slot();
    if (slot == SCHEDULER_MAX_TASKS) {
        scheduler_irq_restore(flags);
        return 0;
    }
    new_task = &tasks[slot];
    if (slot_generation[slot] >= SCHEDULER_MAX_GENERATION) {
        scheduler_irq_restore(flags);
        return 0;
    }
    clear_task(new_task);
    if (!copy_task_name(new_task->name, name)) {
        scheduler_irq_restore(flags);
        return 0;
    }
    new_task->user_entry = entry;
    new_task->user_stack_top = user_stack_top;
    new_task->address_space = address_space;
    new_task->mode = TASK_USER;
    new_task->state = TASK_RUNNABLE;
    slot_generation[slot]++;
    if (slot_generation[slot] == 0U) {
        slot_generation[slot]++;
    }
    new_task->task_id = (slot_generation[slot] << 8U) | slot;
    if (!allocate_task_stack(new_task, slot)) {
        clear_task(new_task);
        scheduler_irq_restore(flags);
        return 0;
    }
    if (slot == task_count) {
        task_count++;
    }
    last_added_task = slot;
    scheduler_irq_restore(flags);
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
    ipc_purge_receiver(scheduler_current_task_id());
    ipc_purge_sender(scheduler_current_task_id());
    tasks[current_task].state = TASK_TERMINATED;
    pending_reap_mask |= 1U << current_task;
    if (!user_process_reap_task(scheduler_current_task_index(), scheduler_current_task_id())) {
        kernel_panic("Unowned user task faulted.");
    }
    return schedule_from_frame(frame, 1);
}

struct interrupt_frame *scheduler_on_user_exit(struct interrupt_frame *frame)
{
    if (!scheduler_started || current_task >= task_count ||
        tasks[current_task].mode != TASK_USER) {
        return frame;
    }
    ipc_purge_receiver(scheduler_current_task_id());
    ipc_purge_sender(scheduler_current_task_id());
    tasks[current_task].state = TASK_TERMINATED;
    pending_reap_mask |= 1U << current_task;
    if (!user_process_reap_task(scheduler_current_task_index(), scheduler_current_task_id())) {
        kernel_panic("Unowned user task exited.");
    }
    return schedule_from_frame(frame, 1);
}

static void scheduler_task_trampoline(void)
{
    scheduler_task_t entry = tasks[current_task].entry;

    if (entry != 0) {
        entry();
    }
    tasks[current_task].state = TASK_TERMINATED;
    pending_reap_mask |= 1U << current_task;
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

unsigned int scheduler_current_task_index(void)
{
    return current_task;
}

unsigned int scheduler_last_added_task(void)
{
    return last_added_task;
}

unsigned int scheduler_current_task_id(void)
{
    if (current_task >= task_count) {
        return 0U;
    }
    return tasks[current_task].task_id;
}

unsigned int scheduler_task_id(unsigned int index)
{
    if (index >= task_count) {
        return 0U;
    }
    return tasks[index].task_id;
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
