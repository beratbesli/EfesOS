#include <stdio.h>

#include "e820.h"

#define TEST_PAGE_SIZE 4096U
#define TEST_PAGE_COUNT 1024U

struct test_state {
    unsigned char usable[TEST_PAGE_COUNT];
    unsigned int events;
    unsigned int usable_events;
    unsigned int reserved_events;
    int reserved_seen;
    int order_valid;
};

static void clear_bytes(void *target, unsigned int size)
{
    unsigned int index;

    for (index = 0U; index < size; index++) {
        ((unsigned char *)target)[index] = 0U;
    }
}

static void make_info(struct boot_info *info)
{
    clear_bytes(info, sizeof(*info));
    info->magic = BOOT_INFO_MAGIC;
    info->boot_drive = 0x80U;
    info->memory_map_entry_count = 2U;
    info->memory_map_entry_size = sizeof(struct boot_memory_map_entry);

    /* Put the reserved record first to prove that input order is irrelevant. */
    info->memory_map[0].base_low = 0x00200000U;
    info->memory_map[0].length_low = 0x00010000U;
    info->memory_map[0].type = 2U;
    info->memory_map[0].attributes = 1U;
    info->memory_map[1].base_low = 0x00100000U;
    info->memory_map[1].length_low = 0x00300000U;
    info->memory_map[1].type = BOOT_MEMORY_AVAILABLE;
    info->memory_map[1].attributes = 1U;
}

static void mark_usable(e820_u64_t base, e820_u64_t length, void *context)
{
    struct test_state *state = (struct test_state *)context;
    e820_u64_t end = base + length;
    unsigned int first = (unsigned int)((base + TEST_PAGE_SIZE - 1U) / TEST_PAGE_SIZE);
    unsigned int last = (unsigned int)(end / TEST_PAGE_SIZE);
    unsigned int page;

    if (state->reserved_seen) {
        state->order_valid = 0;
    }
    state->events++;
    state->usable_events++;
    if (last > TEST_PAGE_COUNT) {
        last = TEST_PAGE_COUNT;
    }
    for (page = first; page < last; page++) {
        state->usable[page] = 1U;
    }
}

static void mark_reserved(e820_u64_t base, e820_u64_t length, void *context)
{
    struct test_state *state = (struct test_state *)context;
    e820_u64_t end = base + length;
    unsigned int first = (unsigned int)(base / TEST_PAGE_SIZE);
    unsigned int last = (unsigned int)((end + TEST_PAGE_SIZE - 1U) / TEST_PAGE_SIZE);
    unsigned int page;

    state->reserved_seen = 1;
    state->events++;
    state->reserved_events++;
    if (last > TEST_PAGE_COUNT) {
        last = TEST_PAGE_COUNT;
    }
    for (page = first; page < last; page++) {
        state->usable[page] = 0U;
    }
}

static int overlap_test(void)
{
    struct boot_info info;
    struct test_state state;

    make_info(&info);
    clear_bytes(&state, sizeof(state));
    state.order_valid = 1;
    if (!e820_apply_memory_map(&info, mark_usable, mark_reserved, &state) ||
        !state.order_valid || state.events != 2U || state.usable_events != 1U ||
        state.reserved_events != 1U || !state.usable[0x00100000U / TEST_PAGE_SIZE] ||
        state.usable[0x00200000U / TEST_PAGE_SIZE] ||
        !state.usable[0x00210000U / TEST_PAGE_SIZE]) {
        return 0;
    }

    /* A disabled E820 record must be ignored rather than reserve memory. */
    info.memory_map[0].attributes = 0U;
    clear_bytes(&state, sizeof(state));
    state.order_valid = 1;
    if (!e820_apply_memory_map(&info, mark_usable, mark_reserved, &state) ||
        state.events != 1U || !state.usable[0x00200000U / TEST_PAGE_SIZE]) {
        return 0;
    }

    /* Unknown active memory types are conservative reserved ranges. */
    info.memory_map[0].attributes = 1U;
    info.memory_map[0].type = 99U;
    clear_bytes(&state, sizeof(state));
    state.order_valid = 1;
    return e820_apply_memory_map(&info, mark_usable, mark_reserved, &state) &&
        !state.usable[0x00200000U / TEST_PAGE_SIZE];
}

int main(void)
{
    struct boot_info info;
    struct test_state state;

    make_info(&info);
    clear_bytes(&state, sizeof(state));
    if (e820_apply_memory_map(0, mark_usable, mark_reserved, &state) ||
        e820_apply_memory_map(&info, 0, mark_reserved, &state) ||
        e820_apply_memory_map(&info, mark_usable, 0, &state) || !overlap_test()) {
        return 1;
    }
    puts("E820 normalization host self-test passed.");
    return 0;
}
