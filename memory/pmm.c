#include "pmm.h"

typedef unsigned long long pmm_u64_t;

#define PMM_MAX_BLOCKS 1048576U
#define PMM_BITMAP_WORDS (PMM_MAX_BLOCKS / 32U)
#define PMM_LOW_MEMORY_END 0x00100000U
#define PMM_BOOTSTRAP_RESERVED_END 0x00100000U
#define PMM_USER_MIN_ADDRESS 0x00400000U
#define PMM_USER_MAX_ADDRESS 0x10000000U
#define PMM_USER_MAX_BLOCKS (PMM_USER_MAX_ADDRESS / PMM_BLOCK_SIZE)
#define PMM_USER_BITMAP_WORDS ((PMM_USER_MAX_BLOCKS + 31U) / 32U)

extern unsigned char __kernel_start;
extern unsigned char __kernel_end;

static pmm_u32_t bitmap[PMM_BITMAP_WORDS];
static pmm_u32_t usable_bitmap[PMM_BITMAP_WORDS];
static pmm_u32_t user_bitmap[PMM_USER_BITMAP_WORDS];
static pmm_u32_t user_mapped_bitmap[PMM_USER_BITMAP_WORDS];
static pmm_u32_t detected_blocks;
static pmm_u32_t managed_blocks;
static pmm_u32_t used_managed_blocks;

static int block_is_used(pmm_u32_t block)
{
    return (bitmap[block / 32U] & (1U << (block % 32U))) != 0U;
}

static int block_is_usable(pmm_u32_t block)
{
    return (usable_bitmap[block / 32U] & (1U << (block % 32U))) != 0U;
}

static void mark_block_used(pmm_u32_t block)
{
    bitmap[block / 32U] |= 1U << (block % 32U);
}

static void mark_block_user_owned(pmm_u32_t block)
{
    if (block >= PMM_USER_MAX_BLOCKS) {
        return;
    }
    user_bitmap[block / 32U] |= 1U << (block % 32U);
}

static void mark_block_kernel_owned(pmm_u32_t block)
{
    if (block >= PMM_USER_MAX_BLOCKS) {
        return;
    }
    user_bitmap[block / 32U] &= ~(1U << (block % 32U));
}

static int user_mapped(pmm_u32_t block)
{
    return block < PMM_USER_MAX_BLOCKS && (user_mapped_bitmap[block / 32U] &
        (1U << (block % 32U))) != 0U;
}

static void mark_user_mapped(pmm_u32_t block)
{
    if (block < PMM_USER_MAX_BLOCKS) {
        user_mapped_bitmap[block / 32U] |= 1U << (block % 32U);
    }
}

static void mark_user_unmapped(pmm_u32_t block)
{
    if (block < PMM_USER_MAX_BLOCKS) {
        user_mapped_bitmap[block / 32U] &= ~(1U << (block % 32U));
    }
}

static void mark_block_free(pmm_u32_t block)
{
    bitmap[block / 32U] &= ~(1U << (block % 32U));
}

static void mark_block_available(pmm_u32_t block)
{
    if (block >= PMM_MAX_BLOCKS || block_is_usable(block)) {
        return;
    }

    usable_bitmap[block / 32U] |= 1U << (block % 32U);
    managed_blocks++;
    mark_block_free(block);
}

static void mark_block_reserved(pmm_u32_t block)
{
    if (block >= PMM_MAX_BLOCKS) {
        return;
    }

    if (block_is_usable(block)) {
        usable_bitmap[block / 32U] &= ~(1U << (block % 32U));
        managed_blocks--;
        if (block_is_used(block)) {
            used_managed_blocks--;
        }
    }
    mark_block_used(block);
}

static pmm_u32_t clamp_block_index(pmm_u64_t value)
{
    if (value >= (pmm_u64_t)PMM_MAX_BLOCKS) {
        return PMM_MAX_BLOCKS;
    }
    return (pmm_u32_t)value;
}

static void release_e820_entry(const struct boot_memory_map_entry *entry)
{
    pmm_u64_t base;
    pmm_u64_t length;
    pmm_u64_t end;
    pmm_u64_t first_block_64;
    pmm_u64_t end_block_64;
    pmm_u32_t first_block;
    pmm_u32_t end_block;
    pmm_u32_t block;

    if (entry->type != BOOT_MEMORY_AVAILABLE || (entry->attributes & 1U) == 0U) {
        return;
    }

    base = ((pmm_u64_t)entry->base_high << 32U) | entry->base_low;
    length = ((pmm_u64_t)entry->length_high << 32U) | entry->length_low;
    end = base + length;
    if (length == 0U || end < base) {
        return;
    }

    /* Round the base up without adding to a near-2^64 value; the latter
       could wrap a hostile, otherwise valid E820 entry into low memory. */
    first_block_64 = base >> 12U;
    if ((base & (PMM_BLOCK_SIZE - 1U)) != 0U) {
        first_block_64++;
    }
    end_block_64 = end >> 12U;
    first_block = clamp_block_index(first_block_64);
    end_block = clamp_block_index(end_block_64);
    if (end_block > detected_blocks) {
        detected_blocks = end_block;
    }

    for (block = first_block; block < end_block; block++) {
        mark_block_available(block);
    }
}

void pmm_reserve_range(pmm_u32_t address, pmm_u32_t length)
{
    pmm_u64_t end = (pmm_u64_t)address + length;
    pmm_u32_t first_block = address / PMM_BLOCK_SIZE;
    pmm_u32_t end_block = clamp_block_index((end + PMM_BLOCK_SIZE - 1U) >> 12U);
    pmm_u32_t block;

    for (block = first_block; block < end_block; block++) {
        mark_block_reserved(block);
    }
}

int pmm_init(const struct boot_info *boot_info)
{
    pmm_u32_t word;
    pmm_u32_t index;
    pmm_u32_t kernel_start = (pmm_u32_t)&__kernel_start;
    pmm_u32_t kernel_end = (pmm_u32_t)&__kernel_end;

    if (!boot_info_is_valid(boot_info) || kernel_end <= kernel_start) {
        return 0;
    }

    for (word = 0; word < PMM_BITMAP_WORDS; word++) {
        bitmap[word] = 0xFFFFFFFFU;
        usable_bitmap[word] = 0U;
    }
    for (word = 0; word < PMM_USER_BITMAP_WORDS; word++) {
        user_bitmap[word] = 0U;
        user_mapped_bitmap[word] = 0U;
    }
    detected_blocks = 0;
    managed_blocks = 0;
    used_managed_blocks = 0;

    for (index = 0; index < boot_info->memory_map_entry_count; index++) {
        release_e820_entry(&boot_info->memory_map[index]);
    }

    pmm_reserve_range(0, PMM_BOOTSTRAP_RESERVED_END);
    pmm_reserve_range(kernel_start, kernel_end - kernel_start);

    if (detected_blocks == 0U || managed_blocks == 0U) {
        return 0;
    }

    return 1;
}

pmm_u32_t pmm_alloc_block_below(pmm_u32_t limit)
{
    pmm_u32_t end_block = limit / PMM_BLOCK_SIZE;
    pmm_u32_t block;

    if (end_block > detected_blocks) {
        end_block = detected_blocks;
    }
    for (block = PMM_LOW_MEMORY_END / PMM_BLOCK_SIZE; block < end_block; block++) {
        if (!block_is_used(block)) {
            mark_block_used(block);
            mark_block_kernel_owned(block);
            used_managed_blocks++;
            return block * PMM_BLOCK_SIZE;
        }
    }

    return 0;
}

pmm_u32_t pmm_alloc_user_block(void)
{
    pmm_u32_t start_block = PMM_USER_MIN_ADDRESS / PMM_BLOCK_SIZE;
    pmm_u32_t end_block = detected_blocks;
    pmm_u32_t block;

    if (end_block > PMM_USER_MAX_BLOCKS) {
        end_block = PMM_USER_MAX_BLOCKS;
    }
    for (block = start_block; block < end_block; block++) {
        if (!block_is_used(block) && block_is_usable(block)) {
            mark_block_used(block);
            mark_block_user_owned(block);
            used_managed_blocks++;
            return block * PMM_BLOCK_SIZE;
        }
    }
    return 0U;
}

int pmm_block_is_user_owned(pmm_u32_t address)
{
    pmm_u32_t block;

    if ((address & (PMM_BLOCK_SIZE - 1U)) != 0U) {
        return 0;
    }
    block = address / PMM_BLOCK_SIZE;
    return block < detected_blocks && block < PMM_USER_MAX_BLOCKS && block_is_usable(block) &&
        block_is_used(block) && (user_bitmap[block / 32U] &
        (1U << (block % 32U))) != 0U;
}

int pmm_claim_user_block(pmm_u32_t address)
{
    pmm_u32_t block;

    if (!pmm_block_is_user_owned(address)) {
        return 0;
    }
    block = address / PMM_BLOCK_SIZE;
    if (user_mapped(block)) {
        return 0;
    }
    mark_user_mapped(block);
    return 1;
}

void pmm_release_user_block(pmm_u32_t address)
{
    if ((address & (PMM_BLOCK_SIZE - 1U)) == 0U) {
        mark_user_unmapped(address / PMM_BLOCK_SIZE);
    }
}

int pmm_user_block_is_mapped(pmm_u32_t address)
{
    pmm_u32_t block;

    if (!pmm_block_is_user_owned(address)) {
        return 0;
    }
    block = address / PMM_BLOCK_SIZE;
    return user_mapped(block);
}

pmm_u32_t pmm_alloc_block(void)
{
    return pmm_alloc_block_below(0xFFFFFFFFU);
}

void pmm_free_block(pmm_u32_t address)
{
    pmm_u32_t block;

    if ((address & (PMM_BLOCK_SIZE - 1U)) != 0U) {
        return;
    }
    block = address / PMM_BLOCK_SIZE;
    if (block < PMM_LOW_MEMORY_END / PMM_BLOCK_SIZE || block >= detected_blocks ||
        !block_is_usable(block) || !block_is_used(block)) {
        return;
    }

    if (user_mapped(block)) {
        return;
    }
    mark_block_free(block);
    mark_block_kernel_owned(block);
    used_managed_blocks--;
}

pmm_u32_t pmm_total_blocks(void)
{
    return managed_blocks;
}

pmm_u32_t pmm_used_blocks(void)
{
    return used_managed_blocks;
}

pmm_u32_t pmm_free_blocks(void)
{
    return managed_blocks - used_managed_blocks;
}

int pmm_self_test(void)
{
    pmm_u32_t free_before = pmm_free_blocks();
    pmm_u32_t first = pmm_alloc_block_below(0x00400000U);
    pmm_u32_t second = pmm_alloc_block_below(0x00400000U);
    pmm_u32_t user = pmm_alloc_user_block();

    if (first == 0U || second == 0U || first == second || user == 0U ||
        pmm_block_is_user_owned(first) || !pmm_block_is_user_owned(user) ||
        pmm_block_is_user_owned(user + 1U)) {
        if (first != 0U) {
            pmm_free_block(first);
        }
        if (second != 0U && second != first) {
            pmm_free_block(second);
        }
        if (user != 0U) {
            pmm_free_block(user);
        }
        return 0;
    }

    pmm_free_block(first);
    pmm_free_block(second);
    pmm_free_block(user);
    return !pmm_block_is_user_owned(user) && pmm_free_blocks() == free_before;
}
