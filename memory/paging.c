#include "paging.h"
#include "pmm.h"

#define PAGE_SIZE 4096U
#define PAGE_TABLE_ENTRIES 1024U
#define PAGE_PRESENT 0x001U
#define PAGE_WRITABLE 0x002U
#define PAGE_ENABLE 0x80000000U

static void load_page_directory(uint32_t address)
{
    __asm__ volatile ("mov %0, %%cr3" : : "r"(address) : "memory");
}

static void enable_paging(void)
{
    uint32_t cr0;

    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= PAGE_ENABLE;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
}

int paging_init(void)
{
    uint32_t directory_address = pmm_alloc_block();
    uint32_t table_address = pmm_alloc_block();
    uint32_t *directory;
    uint32_t *table;
    uint32_t index;

    if (directory_address == 0 || table_address == 0) {
        if (directory_address != 0) {
            pmm_free_block(directory_address);
        }
        if (table_address != 0) {
            pmm_free_block(table_address);
        }
        return 0;
    }

    directory = (uint32_t *)directory_address;
    table = (uint32_t *)table_address;

    for (index = 0; index < PAGE_TABLE_ENTRIES; index++) {
        directory[index] = 0;
        table[index] = (index * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITABLE;
    }

    directory[0] = table_address | PAGE_PRESENT | PAGE_WRITABLE;
    load_page_directory(directory_address);
    enable_paging();
    return 1;
}
