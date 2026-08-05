#include "pmm.h"

#define PMM_BLOCK_SIZE 4096U
#define PMM_MEMORY_SIZE (16U * 1024U * 1024U)
#define PMM_BLOCK_COUNT (PMM_MEMORY_SIZE / PMM_BLOCK_SIZE)
#define PMM_BITMAP_WORDS (PMM_BLOCK_COUNT / 32U)
#define PMM_FIRST_FREE_BLOCK (1024U * 1024U / PMM_BLOCK_SIZE)

static uint32_t bitmap[PMM_BITMAP_WORDS];
static uint32_t used_blocks;

static void set_block_used(uint32_t block)
{
    bitmap[block / 32U] |= 1U << (block % 32U);
}

static void set_block_free(uint32_t block)
{
    bitmap[block / 32U] &= ~(1U << (block % 32U));
}

static int is_block_used(uint32_t block)
{
    return (bitmap[block / 32U] & (1U << (block % 32U))) != 0;
}

void pmm_init(void)
{
    uint32_t word;
    uint32_t block;

    for (word = 0; word < PMM_BITMAP_WORDS; word++) {
        bitmap[word] = 0xFFFFFFFFU;
    }

    used_blocks = PMM_BLOCK_COUNT;

    for (block = PMM_FIRST_FREE_BLOCK; block < PMM_BLOCK_COUNT; block++) {
        set_block_free(block);
        used_blocks--;
    }
}

uint32_t pmm_alloc_block(void)
{
    uint32_t block;

    if (used_blocks == PMM_BLOCK_COUNT) {
        return 0;
    }

    for (block = PMM_FIRST_FREE_BLOCK; block < PMM_BLOCK_COUNT; block++) {
        if (!is_block_used(block)) {
            set_block_used(block);
            used_blocks++;
            return block * PMM_BLOCK_SIZE;
        }
    }

    return 0;
}

void pmm_free_block(uint32_t address)
{
    uint32_t block = address / PMM_BLOCK_SIZE;

    if (block < PMM_FIRST_FREE_BLOCK || block >= PMM_BLOCK_COUNT || !is_block_used(block)) {
        return;
    }

    set_block_free(block);
    used_blocks--;
}

int pmm_self_test(void)
{
    uint32_t first = pmm_alloc_block();
    uint32_t second = pmm_alloc_block();

    if (first == 0) {
        return 0;
    }

    if (second == 0 || first == second) {
        pmm_free_block(first);
        return 0;
    }

    pmm_free_block(first);
    pmm_free_block(second);
    return 1;
}
