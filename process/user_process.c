#include "paging.h"
#include "pmm.h"
#include "scheduler.h"
#include "elf_loader.h"
#include "panic.h"
#include "user_process.h"

#define USER_CODE_ADDRESS 0x00400000U
#define USER_STACK_REGION_BASE 0x00800000U
#define USER_STACK_REGION_STRIDE 0x00100000U
#define USER_STACK_REGION_COUNT 16U
#define USER_PROCESS_MAX 8U

extern unsigned char user_demo_start;
extern unsigned char user_demo_end;

struct user_process_record {
    unsigned int loaded_base;
    unsigned int loaded_end;
    unsigned int stack_frame;
    unsigned int stack_address;
    unsigned int address_space;
    unsigned int task_index;
    unsigned int task_id;
    int active;
};

static struct user_process_record processes[USER_PROCESS_MAX];
static unsigned int process_reaps;

static int user_stack_region_in_use(unsigned int stack_address)
{
    unsigned int index;

    for (index = 0U; index < USER_PROCESS_MAX; index++) {
        if (processes[index].active && processes[index].stack_address == stack_address) {
            return 1;
        }
    }
    return 0;
}

static void copy_bytes(unsigned char *destination, const unsigned char *source, unsigned int length)
{
    unsigned int index;

    for (index = 0; index < length; index++) {
        destination[index] = source[index];
    }
}

static void clear_user_page(unsigned int address)
{
    unsigned int *page = (unsigned int *)address;
    unsigned int index;

    for (index = 0U; index < PAGE_SIZE / sizeof(unsigned int); index++) {
        page[index] = 0U;
    }
}

static void set_u16(unsigned char *data, unsigned int offset, unsigned short value)
{
    data[offset] = (unsigned char)(value & 0xFFU);
    data[offset + 1U] = (unsigned char)(value >> 8U);
}

static void set_u32(unsigned char *data, unsigned int offset, unsigned int value)
{
    data[offset] = (unsigned char)(value & 0xFFU);
    data[offset + 1U] = (unsigned char)((value >> 8U) & 0xFFU);
    data[offset + 2U] = (unsigned char)((value >> 16U) & 0xFFU);
    data[offset + 3U] = (unsigned char)(value >> 24U);
}

static int user_process_spawn_locked(const char *name, const void *image,
    unsigned int image_size)
{
    unsigned int entry = 0U;
    unsigned int loaded_base = 0U;
    unsigned int loaded_end = 0U;
    unsigned int address_space = 0U;
    unsigned int stack_frame = 0U;
    unsigned int stack_address = 0U;
    unsigned int stack_guard_address;
    unsigned int region_offset;
    unsigned int region_index;
    unsigned int process_index;
    unsigned int kernel_directory = paging_kernel_directory();
    unsigned int physical;
    int image_loaded = 0;
    int stack_mapped = 0;

    if (name == 0 || image == 0 || image_size == 0U || image_size > USER_PROCESS_IMAGE_MAX_SIZE ||
        paging_current_directory() != kernel_directory) {
        return 0;
    }
    for (process_index = 0U; process_index < USER_PROCESS_MAX; process_index++) {
        if (!processes[process_index].active) {
            break;
        }
    }
    if (process_index == USER_PROCESS_MAX) {
        return 0;
    }

    region_index = USER_STACK_REGION_COUNT;
    for (region_offset = 0U; region_offset < USER_STACK_REGION_COUNT; region_offset++) {
        unsigned int candidate = (process_reaps + process_index + region_offset) %
            USER_STACK_REGION_COUNT;
        unsigned int candidate_guard = USER_STACK_REGION_BASE +
            (candidate * USER_STACK_REGION_STRIDE);
        unsigned int candidate_stack = candidate_guard + PAGE_SIZE;

        if (!user_stack_region_in_use(candidate_stack) &&
            !paging_is_mapped(candidate_guard) && !paging_is_mapped(candidate_stack)) {
            region_index = candidate;
            break;
        }
    }
    if (region_index == USER_STACK_REGION_COUNT) {
        return 0;
    }
    stack_guard_address = USER_STACK_REGION_BASE + (region_index * USER_STACK_REGION_STRIDE);
    stack_address = stack_guard_address + PAGE_SIZE;
    address_space = paging_create_address_space();
    if (address_space == 0U || !paging_switch_address_space(address_space)) {
        if (address_space != 0U && paging_current_directory() == kernel_directory) {
            paging_destroy_address_space(address_space);
        }
        return 0;
    }
    stack_frame = pmm_alloc_block();
    if (stack_frame == 0U ||
        !paging_map_page(stack_address, stack_frame, PAGE_FLAG_USER | PAGE_FLAG_WRITABLE)) {
        goto cleanup;
    }
    clear_user_page(stack_address);
    stack_mapped = 1;
    image_loaded = elf_load_image(image, image_size, &entry, &loaded_base, &loaded_end);
    /* The guard is intentionally left unmapped. An otherwise valid ELF must
       not be allowed to claim it and silently disable stack-underflow
       isolation. The common cleanup path unloads any such image. */
    if (!image_loaded || paging_is_mapped(stack_guard_address)) {
        goto cleanup;
    }
    if (!paging_switch_address_space(kernel_directory)) {
        kernel_panic("Failed to restore kernel address space.");
    }
    if (!scheduler_add_user_task_in_space(name, entry, stack_address + PAGE_SIZE,
        address_space)) {
        goto cleanup;
    }
    processes[process_index].loaded_base = loaded_base;
    processes[process_index].loaded_end = loaded_end;
    processes[process_index].stack_frame = stack_frame;
    processes[process_index].stack_address = stack_address;
    processes[process_index].address_space = address_space;
    processes[process_index].task_index = scheduler_last_added_task();
    processes[process_index].task_id = scheduler_task_id(processes[process_index].task_index);
    processes[process_index].active = 1;
    return 1;

cleanup:
    if (address_space != 0U && (image_loaded || stack_mapped) &&
        paging_current_directory() != address_space && !paging_switch_address_space(address_space)) {
        kernel_panic("Failed to enter user address space during spawn cleanup.");
    }
    if (image_loaded && !elf_unload_image(loaded_base, loaded_end)) {
        kernel_panic("Failed to unload user ELF image during spawn cleanup.");
    }
    if (stack_mapped) {
        physical = paging_unmap_page(stack_address);
        if (physical == 0U) {
            kernel_panic("Failed to unmap user stack during spawn cleanup.");
        }
        pmm_free_block(physical);
    } else if (stack_frame != 0U) {
        pmm_free_block(stack_frame);
    }
    if (address_space != 0U) {
        if (paging_current_directory() != kernel_directory &&
            !paging_switch_address_space(kernel_directory)) {
            kernel_panic("Failed to restore kernel address space during spawn cleanup.");
        }
        if (!paging_destroy_address_space(address_space)) {
            kernel_panic("Failed to destroy user address space during spawn cleanup.");
        }
    }
    return 0;
}

static int user_process_init_locked(void)
{
    unsigned int code_size = (unsigned int)(&user_demo_end - &user_demo_start);
    unsigned char image[4096];
    unsigned int image_size = 116U + code_size;
    unsigned int index;

    if (code_size == 0U || code_size > PAGE_SIZE || image_size > sizeof(image)) {
        return 0;
    }
    for (index = 0U; index < sizeof(image); index++) {
        image[index] = 0U;
    }
    image[0] = 0x7F;
    image[1] = 'E';
    image[2] = 'L';
    image[3] = 'F';
    image[4] = 1;
    image[5] = 1;
    image[6] = 1;
    set_u16(image, 16U, 2U);
    set_u16(image, 18U, 3U);
    set_u32(image, 20U, 1U);
    set_u32(image, 24U, USER_CODE_ADDRESS);
    set_u32(image, 28U, 52U);
    set_u16(image, 40U, 52U);
    set_u16(image, 42U, 32U);
    set_u16(image, 44U, 1U);
    set_u32(image, 52U, 1U);
    set_u32(image, 56U, 116U);
    set_u32(image, 60U, USER_CODE_ADDRESS);
    set_u32(image, 68U, code_size);
    set_u32(image, 72U, PAGE_SIZE);
    set_u32(image, 76U, 1U);
    set_u32(image, 80U, 1U);
    copy_bytes(image + 116U, &user_demo_start, code_size);
    return user_process_spawn_locked("user-demo", image, image_size);
}

int user_process_guard_self_test(void)
{
    unsigned char image[128];
    unsigned int index;

    for (index = 0U; index < sizeof(image); index++) {
        image[index] = 0U;
    }
    image[0] = 0x7FU;
    image[1] = 'E';
    image[2] = 'L';
    image[3] = 'F';
    image[4] = 1U;
    image[5] = 1U;
    image[6] = 1U;
    set_u16(image, 16U, 2U);
    set_u16(image, 18U, 3U);
    set_u32(image, 20U, 1U);
    set_u32(image, 24U, USER_STACK_REGION_BASE);
    set_u32(image, 28U, 52U);
    set_u16(image, 40U, 52U);
    set_u16(image, 42U, 32U);
    set_u16(image, 44U, 1U);
    set_u32(image, 52U, 1U);
    set_u32(image, 56U, 116U);
    set_u32(image, 60U, USER_STACK_REGION_BASE);
    set_u32(image, 68U, 1U);
    set_u32(image, 72U, PAGE_SIZE);
    set_u32(image, 76U, 1U);
    set_u32(image, 80U, 1U);
    image[116] = 0xC3U;
    return !user_process_spawn("guard-test", image, sizeof(image));
}

int user_process_reap_task(unsigned int task_index, unsigned int task_id)
{
    struct user_process_record *process = 0;
    unsigned int physical;
    unsigned int index;

    for (index = 0U; index < USER_PROCESS_MAX; index++) {
        if (processes[index].active && processes[index].task_index == task_index) {
            process = &processes[index];
            break;
        }
    }
    if (process == 0 || task_id == 0U || process->task_id != task_id) {
        return 0;
    }

    if (process->address_space != 0U && paging_current_directory() != process->address_space) {
        if (!paging_switch_address_space(process->address_space)) {
            kernel_panic("Failed to enter user address space for cleanup.");
        }
    }
    if (process->loaded_base != 0U && process->loaded_end > process->loaded_base) {
        if (!elf_unload_image(process->loaded_base, process->loaded_end)) {
            kernel_panic("Failed to unload user ELF image.");
        }
        process->loaded_base = 0U;
        process->loaded_end = 0U;
    }
    if (process->stack_frame != 0U) {
        if (!paging_is_mapped(process->stack_address) ||
            paging_is_mapped(process->stack_address - PAGE_SIZE)) {
            kernel_panic("User stack mapping disappeared before cleanup.");
        }
        physical = paging_unmap_page(process->stack_address);
        if (physical == 0U) {
            kernel_panic("Failed to unmap user stack.");
        }
        pmm_free_block(physical);
    }
    process->stack_frame = 0U;
    process->stack_address = 0U;
    if (process->address_space != 0U) {
        if (!paging_switch_address_space(paging_kernel_directory()) ||
            !paging_destroy_address_space(process->address_space)) {
            kernel_panic("Failed to destroy user address space.");
        }
        process->address_space = 0U;
    }
    process->task_index = 0U;
    process->task_id = 0U;
    process->active = 0;
    process_reaps++;
    return 1;
}

void user_process_reap(void)
{
    user_process_reap_task(scheduler_current_task_index(), scheduler_current_task_id());
}

unsigned int user_process_reap_count(void)
{
    return process_reaps;
}

int user_process_init(void)
{
    unsigned int flags;
    int result;

    __asm__ volatile ("pushfl\n\tpopl %0\n\tcli" : "=r"(flags) : : "memory");
    result = user_process_init_locked();
    __asm__ volatile ("pushl %0\n\tpopfl" : : "r"(flags) : "memory");
    return result;
}

int user_process_spawn(const char *name, const void *image, unsigned int image_size)
{
    unsigned int flags;
    int result;

    __asm__ volatile ("pushfl\n\tpopl %0\n\tcli" : "=r"(flags) : : "memory");
    result = user_process_spawn_locked(name, image, image_size);
    __asm__ volatile ("pushl %0\n\tpopfl" : : "r"(flags) : "memory");
    return result;
}

unsigned int user_process_address_space(void)
{
    unsigned int index;

    for (index = 0U; index < USER_PROCESS_MAX; index++) {
        if (processes[index].active) {
            return processes[index].address_space;
        }
    }
    return 0U;
}

unsigned int user_process_active_count(void)
{
    unsigned int index;
    unsigned int active = 0U;

    for (index = 0U; index < USER_PROCESS_MAX; index++) {
        if (processes[index].active) {
            active++;
        }
    }
    return active;
}

unsigned int user_process_address_space_at(unsigned int index)
{
    unsigned int process_index;
    unsigned int active_index = 0U;

    for (process_index = 0U; process_index < USER_PROCESS_MAX; process_index++) {
        if (!processes[process_index].active) {
            continue;
        }
        if (active_index++ == index) {
            return processes[process_index].address_space;
        }
    }
    return 0U;
}

unsigned int user_process_stack_address_at(unsigned int index)
{
    unsigned int process_index;
    unsigned int active_index = 0U;

    for (process_index = 0U; process_index < USER_PROCESS_MAX; process_index++) {
        if (!processes[process_index].active) {
            continue;
        }
        if (active_index++ == index) {
            return processes[process_index].stack_address;
        }
    }
    return 0U;
}
