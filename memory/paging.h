#ifndef EFESOS_PAGING_H
#define EFESOS_PAGING_H

#include "boot_info.h"

typedef unsigned int paging_u32_t;

#define PAGE_SIZE 4096U
#define PAGE_FLAG_WRITABLE 0x002U
#define PAGE_FLAG_USER 0x004U

int paging_init(const struct boot_info *boot_info);
int paging_self_test(void);
int paging_map_page(paging_u32_t virtual_address, paging_u32_t physical_address, paging_u32_t flags);
int paging_protect_page(paging_u32_t virtual_address, paging_u32_t flags);
paging_u32_t paging_unmap_page(paging_u32_t virtual_address);
paging_u32_t paging_get_physical(paging_u32_t virtual_address);
int paging_is_mapped(paging_u32_t virtual_address);
int paging_validate_user_range(paging_u32_t virtual_address, paging_u32_t length, int writable);
int paging_copy_from_user(void *destination, paging_u32_t source, paging_u32_t length);

#endif
