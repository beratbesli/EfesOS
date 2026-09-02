#ifndef EFESOS_PMM_H
#define EFESOS_PMM_H

#include "boot_info.h"

typedef unsigned int pmm_u32_t;

#define PMM_BLOCK_SIZE 4096U

int pmm_init(const struct boot_info *boot_info);
pmm_u32_t pmm_alloc_block(void);
pmm_u32_t pmm_alloc_block_below(pmm_u32_t limit);
pmm_u32_t pmm_alloc_user_block(void);
int pmm_block_is_user_owned(pmm_u32_t address);
int pmm_claim_user_block(pmm_u32_t address);
void pmm_release_user_block(pmm_u32_t address);
int pmm_user_block_is_mapped(pmm_u32_t address);
void pmm_free_block(pmm_u32_t address);
void pmm_reserve_range(pmm_u32_t address, pmm_u32_t length);
pmm_u32_t pmm_total_blocks(void);
pmm_u32_t pmm_used_blocks(void);
pmm_u32_t pmm_free_blocks(void);
int pmm_self_test(void);

#endif
