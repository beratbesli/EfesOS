#include <stdio.h>

#include "elf_loader.h"
#include "paging.h"
#include "pmm.h"

/* The host test exercises validation only; mapping primitives are never
   reached by elf_loader_self_test, but stubs keep the link explicit. */
paging_u32_t pmm_alloc_block(void)
{
    return 0U;
}

void pmm_free_block(pmm_u32_t address)
{
    (void)address;
}

int paging_is_mapped(paging_u32_t address)
{
    (void)address;
    return 0;
}

int paging_map_page(paging_u32_t virtual_address, paging_u32_t physical_address,
    paging_u32_t flags)
{
    (void)virtual_address;
    (void)physical_address;
    (void)flags;
    return 0;
}

paging_u32_t paging_unmap_page(paging_u32_t virtual_address)
{
    (void)virtual_address;
    return 0U;
}

int paging_protect_page(paging_u32_t virtual_address, paging_u32_t flags)
{
    (void)virtual_address;
    (void)flags;
    return 0;
}

int main(void)
{
    if (!elf_loader_self_test()) {
        return 1;
    }
    puts("ELF host self-test passed.");
    return 0;
}
