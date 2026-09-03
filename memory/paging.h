#ifndef EFESOS_PAGING_H
#define EFESOS_PAGING_H

#include "boot_info.h"

typedef unsigned int paging_u32_t;

#define PAGE_SIZE 4096U
#define PAGE_FLAG_WRITABLE 0x002U
#define PAGE_FLAG_USER 0x004U
#define PAGE_FLAG_CACHE_DISABLE 0x010U
/* Software execute permission. Bit 9 is an x86 page-table available bit; the
   PAE backend also mirrors this policy into hardware NX when supported. */
#define PAGE_FLAG_EXECUTABLE 0x200U

int paging_init(const struct boot_info *boot_info);
int paging_self_test(void);
int paging_map_page(paging_u32_t virtual_address, paging_u32_t physical_address, paging_u32_t flags);
int paging_protect_page(paging_u32_t virtual_address, paging_u32_t flags);
paging_u32_t paging_unmap_page(paging_u32_t virtual_address);
paging_u32_t paging_get_physical(paging_u32_t virtual_address);
int paging_is_mapped(paging_u32_t virtual_address);
int paging_validate_user_range(paging_u32_t virtual_address, paging_u32_t length, int writable);
int paging_validate_user_execute(paging_u32_t virtual_address);
int paging_copy_from_user(void *destination, paging_u32_t source, paging_u32_t length);
int paging_copy_to_user(paging_u32_t destination, const void *source, paging_u32_t length);
paging_u32_t paging_kernel_directory(void);
paging_u32_t paging_current_directory(void);
int paging_uses_pae(void);
int paging_uses_hardware_nx(void);
unsigned int paging_address_space_extra_blocks(void);
paging_u32_t paging_create_address_space(void);
int paging_address_space_is_valid(paging_u32_t directory);
int paging_switch_address_space(paging_u32_t directory);
int paging_destroy_address_space(paging_u32_t directory);

#endif
