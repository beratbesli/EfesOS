#include "paging.h"
#include "features.h"
#include "pmm.h"

#define PAGE_TABLE_ENTRIES 1024U
#define PAE_TABLE_ENTRIES 512U
#define PAGE_PRESENT 0x001U
#define PAGE_ADDRESS_MASK 0xFFFFF000U
#define PAGE_ALLOWED_FLAGS (PAGE_FLAG_WRITABLE | PAGE_FLAG_USER | PAGE_FLAG_EXECUTABLE)
#define PAGE_ENABLE 0x80000000U
#define PAGE_WRITE_PROTECT 0x00010000U
#define LOW_IDENTITY_LIMIT 0x00400000U
#define VBE_FRAMEBUFFER_VIRTUAL 0xE0000000U
#define VBE_FRAMEBUFFER_PAGES 768U
#define USER_ADDRESS_LIMIT 0xC0000000U
#define USER_MAPPING_MIN 0x00400000U
#define PAGING_MAX_ADDRESS_SPACES 16U
#define PAE_PDP_ENTRIES 4U
#define PAE_PRESENT 0x001ULL
#define PAE_WRITABLE 0x002ULL
#define PAE_USER 0x004ULL
#define PAE_ADDRESS_MASK 0x000FFFFFFFFFF000ULL
#define PAE_NX 0x8000000000000000ULL
#define CR4_PAE 0x00000020U
#define EFER_MSR 0xC0000080U
#define EFER_NXE 0x00000800U

struct pae_entry {
    paging_u32_t low;
    paging_u32_t high;
};

extern unsigned char __text_start;
extern unsigned char __text_end;
extern unsigned char __rodata_end;

static paging_u32_t *page_directory;
static paging_u32_t page_directory_physical;
static paging_u32_t *kernel_page_directory;
static paging_u32_t kernel_page_directory_physical;
static paging_u32_t address_spaces[PAGING_MAX_ADDRESS_SPACES];
static unsigned int address_space_count;
static int paging_enabled;
static int paging_pae_enabled;
static int paging_pae_nx_enabled;
static struct pae_entry *pae_pdpt;
static paging_u32_t pae_pdpt_physical;
static struct pae_entry *pae_kernel_pdpt;
static paging_u32_t pae_kernel_pdpt_physical;
static paging_u32_t pae_address_spaces[PAGING_MAX_ADDRESS_SPACES];
static unsigned int pae_address_space_count;

static void invalidate_page(paging_u32_t virtual_address);
static void flush_tlb(void);

static void clear_page(paging_u32_t *page)
{
    paging_u32_t index;

    for (index = 0; index < PAGE_TABLE_ENTRIES; index++) {
        page[index] = 0;
    }
}

static void clear_pae_page(struct pae_entry *page)
{
    unsigned int index;

    for (index = 0U; index < PAE_TABLE_ENTRIES; index++) {
        page[index].low = 0U;
        page[index].high = 0U;
    }
}

static unsigned long long pae_entry_value(const struct pae_entry *entry)
{
    return ((unsigned long long)entry->high << 32U) | entry->low;
}

static void pae_set_entry(struct pae_entry *entry, unsigned long long value)
{
    entry->low = (paging_u32_t)value;
    entry->high = (paging_u32_t)(value >> 32U);
}

static paging_u32_t pae_entry_physical(const struct pae_entry *entry)
{
    return (paging_u32_t)(pae_entry_value(entry) & PAE_ADDRESS_MASK);
}

static int pae_entry_present(const struct pae_entry *entry)
{
    return (pae_entry_value(entry) & PAE_PRESENT) != 0U;
}

static unsigned long long pae_flags_to_entry(paging_u32_t physical_address,
    paging_u32_t flags)
{
    unsigned long long value = (unsigned long long)physical_address |
        PAE_PRESENT | ((flags & PAGE_FLAG_WRITABLE) != 0U ? PAE_WRITABLE : 0U) |
        ((flags & PAGE_FLAG_USER) != 0U ? PAE_USER : 0U);

    if ((flags & PAGE_FLAG_EXECUTABLE) != 0U) {
        value |= (unsigned long long)PAGE_FLAG_EXECUTABLE;
    }
    if (paging_pae_nx_enabled && (flags & PAGE_FLAG_EXECUTABLE) == 0U) {
        value |= PAE_NX;
    }
    return value;
}

static struct pae_entry *pae_page_directory_for(paging_u32_t directory,
    unsigned int pdpt_index)
{
    struct pae_entry *directory_table = (struct pae_entry *)directory;
    if (directory == 0U || pdpt_index >= PAE_PDP_ENTRIES ||
        !pae_entry_present(&directory_table[pdpt_index])) {
        return 0;
    }
    return (struct pae_entry *)pae_entry_physical(&directory_table[pdpt_index]);
}

static struct pae_entry *pae_get_page_table_for(paging_u32_t directory,
    paging_u32_t virtual_address)
{
    struct pae_entry *page_directory = pae_page_directory_for(directory,
        virtual_address >> 30U);
    struct pae_entry *entry;

    if (page_directory == 0) {
        return 0;
    }
    entry = &page_directory[(virtual_address >> 21U) & 0x1FFU];
    if (!pae_entry_present(entry)) {
        return 0;
    }
    return (struct pae_entry *)pae_entry_physical(entry);
}

static struct pae_entry *pae_get_page_table(paging_u32_t virtual_address)
{
    return pae_get_page_table_for(pae_pdpt_physical, virtual_address);
}

static int pae_uses_shared_kernel_table(paging_u32_t virtual_address)
{
    struct pae_entry *kernel_directory = pae_page_directory_for(
        pae_kernel_pdpt_physical, virtual_address >> 30U);
    struct pae_entry *current_directory = pae_page_directory_for(
        pae_pdpt_physical, virtual_address >> 30U);
    unsigned int index = (virtual_address >> 21U) & 0x1FFU;

    if (pae_pdpt == 0 || pae_kernel_pdpt == 0 ||
        pae_pdpt_physical == pae_kernel_pdpt_physical ||
        kernel_directory == 0 || current_directory == 0) {
        return 0;
    }
    return pae_entry_present(&kernel_directory[index]) &&
        pae_entry_present(&current_directory[index]) &&
        pae_entry_physical(&kernel_directory[index]) ==
            pae_entry_physical(&current_directory[index]);
}

static int pae_page_table_allows_user_mappings(const struct pae_entry *table)
{
    unsigned int index;

    if (table == 0) {
        return 0;
    }
    for (index = 0U; index < PAE_TABLE_ENTRIES; index++) {
        if (pae_entry_present(&table[index]) &&
            (pae_entry_value(&table[index]) & PAE_USER) == 0U) {
            return 0;
        }
    }
    return 1;
}

static struct pae_entry *pae_ensure_page_table(paging_u32_t virtual_address,
    paging_u32_t flags)
{
    unsigned int pdpt_index = virtual_address >> 30U;
    struct pae_entry *page_directory = pae_page_directory_for(pae_pdpt_physical,
        pdpt_index);
    unsigned int index = (virtual_address >> 21U) & 0x1FFU;
    struct pae_entry *entry;
    paging_u32_t physical;

    if (page_directory == 0) {
        struct pae_entry *pdpt_entry;

        if (pdpt_index >= PAE_PDP_ENTRIES) {
            return 0;
        }
        physical = pmm_alloc_block_below(LOW_IDENTITY_LIMIT);
        if (physical == 0U) {
            return 0;
        }
        clear_pae_page((struct pae_entry *)physical);
        pdpt_entry = &pae_pdpt[pdpt_index];
        pae_set_entry(pdpt_entry, (unsigned long long)physical | PAE_PRESENT);
        page_directory = (struct pae_entry *)physical;
    }
    entry = &page_directory[index];
    if (pae_entry_present(entry)) {
        if (pae_uses_shared_kernel_table(virtual_address)) {
            return 0;
        }
        if ((flags & PAGE_FLAG_USER) != 0U) {
            struct pae_entry *table = (struct pae_entry *)pae_entry_physical(entry);
            if (!pae_page_table_allows_user_mappings(table)) {
                return 0;
            }
            pae_set_entry(entry, pae_entry_value(entry) | PAE_USER);
        }
        return (struct pae_entry *)pae_entry_physical(entry);
    }
    physical = pmm_alloc_block_below(LOW_IDENTITY_LIMIT);
    if (physical == 0U) {
        return 0;
    }
    clear_pae_page((struct pae_entry *)physical);
    pae_set_entry(entry, (unsigned long long)physical | PAE_PRESENT |
        PAE_WRITABLE | ((flags & PAGE_FLAG_USER) != 0U ? PAE_USER : 0U));
    return (struct pae_entry *)physical;
}

static int pae_map_page(paging_u32_t virtual_address, paging_u32_t physical_address,
    paging_u32_t flags)
{
    struct pae_entry *table;
    struct pae_entry *entry;

    if (pae_pdpt == 0 || virtual_address < PAGE_SIZE || physical_address < PAGE_SIZE ||
        (virtual_address & (PAGE_SIZE - 1U)) != 0U ||
        (physical_address & (PAGE_SIZE - 1U)) != 0U ||
        (flags & ~PAGE_ALLOWED_FLAGS) != 0U ||
        (flags & (PAGE_FLAG_WRITABLE | PAGE_FLAG_EXECUTABLE)) ==
            (PAGE_FLAG_WRITABLE | PAGE_FLAG_EXECUTABLE) ||
        ((flags & PAGE_FLAG_USER) != 0U && !pmm_block_is_user_owned(physical_address)) ||
        (pae_pdpt_physical == pae_kernel_pdpt_physical &&
            (flags & PAGE_FLAG_USER) != 0U) || pae_uses_shared_kernel_table(virtual_address) ||
        ((flags & PAGE_FLAG_USER) != 0U &&
            (virtual_address < USER_MAPPING_MIN || virtual_address >= USER_ADDRESS_LIMIT))) {
        return 0;
    }
    table = pae_ensure_page_table(virtual_address, flags);
    if (table == 0) {
        return 0;
    }
    entry = &table[(virtual_address >> 12U) & 0x1FFU];
    if (pae_entry_present(entry)) {
        return 0;
    }
    if ((flags & PAGE_FLAG_USER) != 0U && !pmm_claim_user_block(physical_address)) {
        return 0;
    }
    pae_set_entry(entry, pae_flags_to_entry(physical_address, flags));
    invalidate_page(virtual_address);
    return 1;
}

static int pae_protect_page(paging_u32_t virtual_address, paging_u32_t flags)
{
    struct pae_entry *table;
    struct pae_entry *entry;
    unsigned long long value;

    if (pae_pdpt == 0 || virtual_address < PAGE_SIZE ||
        (virtual_address & (PAGE_SIZE - 1U)) != 0U ||
        (flags & (PAGE_FLAG_WRITABLE | PAGE_FLAG_EXECUTABLE)) ==
            (PAGE_FLAG_WRITABLE | PAGE_FLAG_EXECUTABLE) ||
        (pae_pdpt_physical == pae_kernel_pdpt_physical &&
            (flags & PAGE_FLAG_USER) != 0U) || pae_uses_shared_kernel_table(virtual_address) ||
        ((flags & PAGE_FLAG_USER) != 0U &&
            (virtual_address < USER_MAPPING_MIN || virtual_address >= USER_ADDRESS_LIMIT)) ||
        (flags & ~PAGE_ALLOWED_FLAGS) != 0U) {
        return 0;
    }
    table = pae_get_page_table(virtual_address);
    if (table == 0) {
        return 0;
    }
    entry = &table[(virtual_address >> 12U) & 0x1FFU];
    value = pae_entry_value(entry);
    if ((value & PAE_PRESENT) == 0U ||
        ((value & PAE_USER) != 0U) != ((flags & PAGE_FLAG_USER) != 0U)) {
        return 0;
    }
    if ((flags & PAGE_FLAG_USER) != 0U &&
        !pmm_block_is_user_owned(pae_entry_physical(entry))) {
        return 0;
    }
    pae_set_entry(entry, pae_flags_to_entry(pae_entry_physical(entry), flags));
    if ((flags & PAGE_FLAG_USER) != 0U) {
        struct pae_entry *directory = pae_page_directory_for(pae_pdpt_physical,
            virtual_address >> 30U);
        directory[(virtual_address >> 21U) & 0x1FFU].low |= PAE_USER;
    }
    invalidate_page(virtual_address);
    return 1;
}

static paging_u32_t pae_unmap_page(paging_u32_t virtual_address)
{
    struct pae_entry *table;
    struct pae_entry *entry;
    struct pae_entry *directory;
    unsigned int index;
    paging_u32_t physical;
    unsigned long long value;

    if (pae_pdpt == 0 || virtual_address < PAGE_SIZE ||
        (virtual_address & (PAGE_SIZE - 1U)) != 0U ||
        (pae_pdpt_physical == pae_kernel_pdpt_physical && virtual_address < USER_MAPPING_MIN) ||
        pae_uses_shared_kernel_table(virtual_address)) {
        return 0;
    }
    table = pae_get_page_table(virtual_address);
    if (table == 0) {
        return 0;
    }
    entry = &table[(virtual_address >> 12U) & 0x1FFU];
    if (!pae_entry_present(entry)) {
        return 0;
    }
    value = pae_entry_value(entry);
    physical = pae_entry_physical(entry);
    pae_set_entry(entry, 0U);
    if ((value & PAE_USER) != 0U) {
        pmm_release_user_block(physical);
    }
    invalidate_page(virtual_address);
    for (index = 0U; index < PAE_TABLE_ENTRIES; index++) {
        if (pae_entry_present(&table[index])) {
            return physical;
        }
    }
    directory = pae_page_directory_for(pae_pdpt_physical, virtual_address >> 30U);
    pae_set_entry(&directory[(virtual_address >> 21U) & 0x1FFU], 0U);
    flush_tlb();
    pmm_free_block((paging_u32_t)table);
    for (index = 0U; index < PAE_TABLE_ENTRIES; index++) {
        if (pae_entry_present(&directory[index])) {
            return physical;
        }
    }
    pae_set_entry(&pae_pdpt[virtual_address >> 30U], 0U);
    pmm_free_block((paging_u32_t)directory);
    return physical;
}

static paging_u32_t pae_get_physical(paging_u32_t virtual_address)
{
    struct pae_entry *table = pae_get_page_table(virtual_address);
    if (table == 0 || !pae_entry_present(&table[(virtual_address >> 12U) & 0x1FFU])) {
        return 0U;
    }
    return pae_entry_physical(&table[(virtual_address >> 12U) & 0x1FFU]) |
        (virtual_address & (PAGE_SIZE - 1U));
}

static int pae_is_mapped(paging_u32_t virtual_address)
{
    struct pae_entry *table = pae_get_page_table(virtual_address);
    return table != 0 && pae_entry_present(&table[(virtual_address >> 12U) & 0x1FFU]);
}

static void invalidate_page(paging_u32_t virtual_address)
{
    if (paging_enabled) {
        __asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
    }
}

static void flush_tlb(void)
{
    if (paging_enabled) {
        __asm__ volatile ("mov %0, %%cr3" : : "r"(page_directory_physical) : "memory");
    }
}

static int address_space_is_registered(paging_u32_t directory)
{
    unsigned int index;

    if (directory == kernel_page_directory_physical) {
        return 1;
    }
    for (index = 0U; index < address_space_count; index++) {
        if (address_spaces[index] == directory) {
            return 1;
        }
    }
    return 0;
}

static int pae_address_space_is_registered(paging_u32_t directory)
{
    unsigned int index;

    if (directory == pae_kernel_pdpt_physical) {
        return 1;
    }
    for (index = 0U; index < pae_address_space_count; index++) {
        if (pae_address_spaces[index] == directory) {
            return 1;
        }
    }
    return 0;
}

static int uses_shared_kernel_table(paging_u32_t virtual_address)
{
    paging_u32_t directory_index;
    paging_u32_t kernel_entry;
    paging_u32_t current_entry;

    if (page_directory == 0 || kernel_page_directory == 0 ||
        page_directory == kernel_page_directory) {
        return 0;
    }
    directory_index = virtual_address >> 22U;
    kernel_entry = kernel_page_directory[directory_index];
    current_entry = page_directory[directory_index];
    return (kernel_entry & PAGE_PRESENT) != 0U &&
        (current_entry & PAGE_PRESENT) != 0U &&
        (kernel_entry & PAGE_ADDRESS_MASK) == (current_entry & PAGE_ADDRESS_MASK);
}

static int page_table_allows_user_mappings(const paging_u32_t *table)
{
    unsigned int index;

    if (table == 0) {
        return 0;
    }
    /* A PDE user bit applies to every PTE in the table. Keep the table
       homogeneous so a later user mapping can never change the effective
       privilege boundary of an existing supervisor PTE. */
    for (index = 0U; index < PAGE_TABLE_ENTRIES; index++) {
        if ((table[index] & PAGE_PRESENT) != 0U &&
            (table[index] & PAGE_FLAG_USER) == 0U) {
            return 0;
        }
    }
    return 1;
}

static paging_u32_t *get_page_table(paging_u32_t virtual_address)
{
    paging_u32_t entry;

    if (page_directory == 0) {
        return 0;
    }
    entry = page_directory[virtual_address >> 22U];

    if ((entry & PAGE_PRESENT) == 0U) {
        return 0;
    }
    return (paging_u32_t *)(entry & PAGE_ADDRESS_MASK);
}

static paging_u32_t *ensure_page_table(paging_u32_t virtual_address, paging_u32_t flags)
{
    paging_u32_t directory_index = virtual_address >> 22U;
    paging_u32_t entry = page_directory[directory_index];
    paging_u32_t table_physical;
    paging_u32_t *table;

    if ((entry & PAGE_PRESENT) != 0U) {
        if (uses_shared_kernel_table(virtual_address)) {
            return 0;
        }
        if ((flags & PAGE_FLAG_USER) != 0U) {
            if (!page_table_allows_user_mappings((paging_u32_t *)(entry & PAGE_ADDRESS_MASK))) {
                return 0;
            }
            page_directory[directory_index] |= PAGE_FLAG_USER;
        }
        return (paging_u32_t *)(entry & PAGE_ADDRESS_MASK);
    }

    table_physical = pmm_alloc_block_below(LOW_IDENTITY_LIMIT);
    if (table_physical == 0U) {
        return 0;
    }

    table = (paging_u32_t *)table_physical;
    clear_page(table);
    page_directory[directory_index] = table_physical | PAGE_PRESENT | PAGE_FLAG_WRITABLE |
                                      (flags & PAGE_FLAG_USER);
    return table;
}

int paging_map_page(paging_u32_t virtual_address, paging_u32_t physical_address, paging_u32_t flags)
{
    paging_u32_t *table;
    paging_u32_t table_index;

    if (paging_pae_enabled) {
        return pae_map_page(virtual_address, physical_address, flags);
    }

    if (page_directory == 0 || virtual_address < PAGE_SIZE || physical_address < PAGE_SIZE ||
        (virtual_address & (PAGE_SIZE - 1U)) != 0U ||
        (physical_address & (PAGE_SIZE - 1U)) != 0U ||
        (flags & ~PAGE_ALLOWED_FLAGS) != 0U ||
        (flags & (PAGE_FLAG_WRITABLE | PAGE_FLAG_EXECUTABLE)) ==
            (PAGE_FLAG_WRITABLE | PAGE_FLAG_EXECUTABLE) ||
        ((flags & PAGE_FLAG_USER) != 0U && !pmm_block_is_user_owned(physical_address)) ||
        (page_directory == kernel_page_directory && (flags & PAGE_FLAG_USER) != 0U) ||
        uses_shared_kernel_table(virtual_address) ||
        ((flags & PAGE_FLAG_USER) != 0U &&
         (virtual_address < USER_MAPPING_MIN || virtual_address >= USER_ADDRESS_LIMIT))) {
        return 0;
    }

    table = ensure_page_table(virtual_address, flags);
    if (table == 0) {
        return 0;
    }
    table_index = (virtual_address >> 12U) & 0x3FFU;
    if ((table[table_index] & PAGE_PRESENT) != 0U) {
        return 0;
    }

    if ((flags & PAGE_FLAG_USER) != 0U && !pmm_claim_user_block(physical_address)) {
        return 0;
    }

    table[table_index] = physical_address | PAGE_PRESENT | (flags & PAGE_ALLOWED_FLAGS);
    invalidate_page(virtual_address);
    return 1;
}

int paging_protect_page(paging_u32_t virtual_address, paging_u32_t flags)
{
    paging_u32_t *table;
    paging_u32_t table_index;
    paging_u32_t entry;

    if (paging_pae_enabled) {
        return pae_protect_page(virtual_address, flags);
    }

    if (page_directory == 0 || virtual_address < PAGE_SIZE ||
        (virtual_address & (PAGE_SIZE - 1U)) != 0U ||
        (flags & (PAGE_FLAG_WRITABLE | PAGE_FLAG_EXECUTABLE)) ==
            (PAGE_FLAG_WRITABLE | PAGE_FLAG_EXECUTABLE) ||
        (page_directory == kernel_page_directory && (flags & PAGE_FLAG_USER) != 0U) ||
        uses_shared_kernel_table(virtual_address) ||
        ((flags & PAGE_FLAG_USER) != 0U &&
         (virtual_address < USER_MAPPING_MIN || virtual_address >= USER_ADDRESS_LIMIT)) ||
        (flags & ~PAGE_ALLOWED_FLAGS) != 0U) {
        return 0;
    }
    table = get_page_table(virtual_address);
    if (table == 0) {
        return 0;
    }
    table_index = (virtual_address >> 12U) & 0x3FFU;
    entry = table[table_index];
    if ((entry & PAGE_PRESENT) == 0U) {
        return 0;
    }
    /* A mapping cannot silently cross the kernel/user ownership boundary;
       doing so would orphan a claimed user frame or expose a kernel frame. */
    if ((entry & PAGE_FLAG_USER) != (flags & PAGE_FLAG_USER)) {
        return 0;
    }
    if ((flags & PAGE_FLAG_USER) != 0U &&
        !pmm_block_is_user_owned(entry & PAGE_ADDRESS_MASK)) {
        return 0;
    }
    table[table_index] = (entry & PAGE_ADDRESS_MASK) | PAGE_PRESENT | flags;
    if ((flags & PAGE_FLAG_USER) != 0U) {
        page_directory[virtual_address >> 22U] |= PAGE_FLAG_USER;
    }
    invalidate_page(virtual_address);
    return 1;
}

paging_u32_t paging_unmap_page(paging_u32_t virtual_address)
{
    paging_u32_t *table;
    paging_u32_t directory_index;
    paging_u32_t table_index;
    paging_u32_t physical_address;
    paging_u32_t index;
    paging_u32_t entry;

    if (paging_pae_enabled) {
        return pae_unmap_page(virtual_address);
    }

    if (virtual_address < PAGE_SIZE || (virtual_address & (PAGE_SIZE - 1U)) != 0U ||
        (page_directory == kernel_page_directory && virtual_address < USER_MAPPING_MIN) ||
        uses_shared_kernel_table(virtual_address)) {
        return 0;
    }

    directory_index = virtual_address >> 22U;
    table = get_page_table(virtual_address);
    if (table == 0) {
        return 0;
    }
    table_index = (virtual_address >> 12U) & 0x3FFU;
    if ((table[table_index] & PAGE_PRESENT) == 0U) {
        return 0;
    }

    entry = table[table_index];
    physical_address = entry & PAGE_ADDRESS_MASK;
    table[table_index] = 0;
    if ((entry & PAGE_FLAG_USER) != 0U) {
        pmm_release_user_block(physical_address);
    }
    invalidate_page(virtual_address);

    for (index = 0; index < PAGE_TABLE_ENTRIES; index++) {
        if ((table[index] & PAGE_PRESENT) != 0U) {
            return physical_address;
        }
    }

    page_directory[directory_index] = 0;
    flush_tlb();
    pmm_free_block((paging_u32_t)table);
    return physical_address;
}

paging_u32_t paging_get_physical(paging_u32_t virtual_address)
{
    paging_u32_t *table = get_page_table(virtual_address);
    paging_u32_t entry;

    if (paging_pae_enabled) {
        return pae_get_physical(virtual_address);
    }

    if (table == 0) {
        return 0;
    }
    entry = table[(virtual_address >> 12U) & 0x3FFU];
    if ((entry & PAGE_PRESENT) == 0U) {
        return 0;
    }
    return (entry & PAGE_ADDRESS_MASK) | (virtual_address & (PAGE_SIZE - 1U));
}

int paging_is_mapped(paging_u32_t virtual_address)
{
    paging_u32_t *table = get_page_table(virtual_address);

    if (paging_pae_enabled) {
        return pae_is_mapped(virtual_address);
    }

    if (table == 0) {
        return 0;
    }
    return (table[(virtual_address >> 12U) & 0x3FFU] & PAGE_PRESENT) != 0U;
}

int paging_validate_user_range(paging_u32_t virtual_address, paging_u32_t length, int writable)
{
    paging_u32_t end;
    paging_u32_t page;

    if (paging_pae_enabled) {
        paging_u32_t end_page;

        if (length == 0U) {
            return 1;
        }
        if (pae_pdpt == 0 || virtual_address < PAGE_SIZE ||
            virtual_address >= USER_ADDRESS_LIMIT ||
            length > USER_ADDRESS_LIMIT - virtual_address) {
            return 0;
        }
        end_page = virtual_address + length;
        page = virtual_address & PAGE_ADDRESS_MASK;
        while (page < end_page) {
            struct pae_entry *directory = pae_page_directory_for(pae_pdpt_physical,
                page >> 30U);
            struct pae_entry *table = pae_get_page_table(page);
            struct pae_entry *entry;
            unsigned long long value;

            if (directory == 0 || table == 0 ||
                (pae_entry_value(&directory[(page >> 21U) & 0x1FFU]) &
                    (PAE_PRESENT | PAE_USER)) != (PAE_PRESENT | PAE_USER)) {
                return 0;
            }
            entry = &table[(page >> 12U) & 0x1FFU];
            value = pae_entry_value(entry);
            if ((value & (PAE_PRESENT | PAE_USER)) != (PAE_PRESENT | PAE_USER) ||
                (writable != 0 && (value & PAE_WRITABLE) == 0U)) {
                return 0;
            }
            page += PAGE_SIZE;
        }
        return 1;
    }

    if (length == 0U) {
        return 1;
    }
    if (page_directory == 0 || virtual_address < PAGE_SIZE || virtual_address >= USER_ADDRESS_LIMIT ||
        length > USER_ADDRESS_LIMIT - virtual_address) {
        return 0;
    }
    end = virtual_address + length;
    page = virtual_address & PAGE_ADDRESS_MASK;
    while (page < end) {
        paging_u32_t directory_entry = page_directory[page >> 22U];
        paging_u32_t *table = get_page_table(page);
        paging_u32_t entry;

        if (table == 0 || (directory_entry & (PAGE_PRESENT | PAGE_FLAG_USER)) !=
            (PAGE_PRESENT | PAGE_FLAG_USER)) {
            return 0;
        }
        entry = table[(page >> 12U) & 0x3FFU];
        if ((entry & PAGE_PRESENT) == 0U || (entry & PAGE_FLAG_USER) == 0U ||
            (writable != 0 && (entry & PAGE_FLAG_WRITABLE) == 0U)) {
            return 0;
        }
        page += PAGE_SIZE;
    }
    return 1;
}

int paging_validate_user_execute(paging_u32_t virtual_address)
{
    paging_u32_t directory_entry;
    paging_u32_t *table;
    paging_u32_t entry;

    if (paging_pae_enabled) {
        struct pae_entry *directory;
        struct pae_entry *table;
        unsigned long long value;

        if (pae_pdpt == 0 || virtual_address < PAGE_SIZE ||
            virtual_address >= USER_ADDRESS_LIMIT) {
            return 0;
        }
        directory = pae_page_directory_for(pae_pdpt_physical, virtual_address >> 30U);
        table = pae_get_page_table(virtual_address);
        if (directory == 0 || table == 0 ||
            (pae_entry_value(&directory[(virtual_address >> 21U) & 0x1FFU]) &
                (PAE_PRESENT | PAE_USER)) != (PAE_PRESENT | PAE_USER)) {
            return 0;
        }
        value = pae_entry_value(&table[(virtual_address >> 12U) & 0x1FFU]);
        return (value & (PAE_PRESENT | PAE_USER | PAE_WRITABLE)) ==
            (PAE_PRESENT | PAE_USER) &&
            (value & (1ULL << 9U)) != 0U &&
            (!paging_pae_nx_enabled || (value & PAE_NX) == 0U);
    }

    if (page_directory == 0 || virtual_address < PAGE_SIZE || virtual_address >= USER_ADDRESS_LIMIT) {
        return 0;
    }
    directory_entry = page_directory[virtual_address >> 22U];
    if ((directory_entry & (PAGE_PRESENT | PAGE_FLAG_USER)) !=
        (PAGE_PRESENT | PAGE_FLAG_USER)) {
        return 0;
    }
    table = get_page_table(virtual_address);
    if (table == 0) {
        return 0;
    }
    entry = table[(virtual_address >> 12U) & 0x3FFU];
    return (entry & (PAGE_PRESENT | PAGE_FLAG_USER | PAGE_FLAG_WRITABLE |
        PAGE_FLAG_EXECUTABLE)) == (PAGE_PRESENT | PAGE_FLAG_USER | PAGE_FLAG_EXECUTABLE);
}

int paging_copy_from_user(void *destination, paging_u32_t source, paging_u32_t length)
{
    unsigned char *output = (unsigned char *)destination;
    const unsigned char *input = (const unsigned char *)source;
    paging_u32_t index;

    if (length != 0U && output == 0) {
        return 0;
    }
    if (!paging_validate_user_range(source, length, 0)) {
        return 0;
    }
    for (index = 0; index < length; index++) {
        output[index] = input[index];
    }
    return 1;
}

int paging_copy_to_user(paging_u32_t destination, const void *source, paging_u32_t length)
{
    const unsigned char *input = (const unsigned char *)source;
    unsigned char *output = (unsigned char *)destination;
    paging_u32_t index;

    if (length != 0U && input == 0) {
        return 0;
    }
    if (!paging_validate_user_range(destination, length, 1)) {
        return 0;
    }
    for (index = 0; index < length; index++) {
        output[index] = input[index];
    }
    return 1;
}

paging_u32_t paging_kernel_directory(void)
{
    return kernel_page_directory_physical;
}

paging_u32_t paging_current_directory(void)
{
    return page_directory_physical;
}

int paging_uses_pae(void)
{
    return paging_pae_enabled;
}

int paging_uses_hardware_nx(void)
{
    return paging_pae_nx_enabled;
}

unsigned int paging_address_space_extra_blocks(void)
{
    unsigned int index;
    unsigned int count = 0U;

    if (!paging_pae_enabled || pae_kernel_pdpt == 0) {
        return 0U;
    }
    for (index = 0U; index < PAE_PDP_ENTRIES; index++) {
        if (pae_entry_present(&pae_kernel_pdpt[index])) {
            count++;
        }
    }
    return count;
}

paging_u32_t paging_create_address_space(void)
{
    paging_u32_t physical;
    paging_u32_t *directory;
    paging_u32_t index;

    if (paging_pae_enabled) {
        struct pae_entry *directory;
        struct pae_entry *kernel_directory;
        paging_u32_t physical;
        unsigned int pdpt_index;

        if (pae_kernel_pdpt == 0 || pae_address_space_count == PAGING_MAX_ADDRESS_SPACES) {
            return 0U;
        }
        physical = pmm_alloc_block_below(LOW_IDENTITY_LIMIT);
        if (physical == 0U) {
            return 0U;
        }
        directory = (struct pae_entry *)physical;
        clear_pae_page(directory);
        for (pdpt_index = 0U; pdpt_index < PAE_PDP_ENTRIES; pdpt_index++) {
            paging_u32_t private_physical;

            if (!pae_entry_present(&pae_kernel_pdpt[pdpt_index])) {
                continue;
            }
            private_physical = pmm_alloc_block_below(LOW_IDENTITY_LIMIT);
            if (private_physical == 0U) {
                for (index = 0U; index < pdpt_index; index++) {
                    if (pae_entry_present(&directory[index])) {
                        pmm_free_block(pae_entry_physical(&directory[index]));
                    }
                }
                pmm_free_block(physical);
                return 0U;
            }
            kernel_directory = (struct pae_entry *)pae_entry_physical(
                &pae_kernel_pdpt[pdpt_index]);
            for (index = 0U; index < PAE_TABLE_ENTRIES; index++) {
                ((struct pae_entry *)private_physical)[index] = kernel_directory[index];
            }
            pae_set_entry(&directory[pdpt_index],
                (unsigned long long)private_physical | PAE_PRESENT);
        }
        pae_address_spaces[pae_address_space_count++] = physical;
        return physical;
    }

    if (kernel_page_directory == 0 || address_space_count == PAGING_MAX_ADDRESS_SPACES) {
        return 0;
    }
    physical = pmm_alloc_block_below(LOW_IDENTITY_LIMIT);
    if (physical == 0U) {
        return 0;
    }
    directory = (paging_u32_t *)physical;
    for (index = 0; index < PAGE_TABLE_ENTRIES; index++) {
        directory[index] = kernel_page_directory[index];
    }
    address_spaces[address_space_count++] = physical;
    return physical;
}

int paging_address_space_is_valid(paging_u32_t directory)
{
    if (paging_pae_enabled) {
        return directory != 0U && (directory & (PAGE_SIZE - 1U)) == 0U &&
            pae_address_space_is_registered(directory);
    }
    return directory != 0U && (directory & (PAGE_SIZE - 1U)) == 0U &&
        address_space_is_registered(directory);
}

int paging_switch_address_space(paging_u32_t directory)
{
    if (paging_pae_enabled) {
        if (directory == 0U || (directory & (PAGE_SIZE - 1U)) != 0U ||
            !pae_address_space_is_registered(directory)) {
            return 0;
        }
        if (directory == pae_pdpt_physical) {
            return 1;
        }
        pae_pdpt_physical = directory;
        pae_pdpt = (struct pae_entry *)directory;
        page_directory_physical = directory;
        flush_tlb();
        return 1;
    }
    if (directory == 0U || (directory & (PAGE_SIZE - 1U)) != 0U ||
        !address_space_is_registered(directory)) {
        return 0;
    }
    if (directory == page_directory_physical) {
        return directory == page_directory_physical;
    }
    page_directory_physical = directory;
    page_directory = (paging_u32_t *)directory;
    flush_tlb();
    return 1;
}

int paging_destroy_address_space(paging_u32_t directory)
{
    paging_u32_t *space;
    paging_u32_t index;

    if (paging_pae_enabled) {
        struct pae_entry *space;

        if (directory == 0U || directory == pae_kernel_pdpt_physical ||
            directory == pae_pdpt_physical || !paging_address_space_is_valid(directory)) {
            return 0;
        }
        space = (struct pae_entry *)directory;
        for (index = 0U; index < PAE_PDP_ENTRIES; index++) {
            struct pae_entry *kernel_directory = pae_page_directory_for(
                pae_kernel_pdpt_physical, index);
            struct pae_entry *table_directory = pae_page_directory_for(directory, index);
            unsigned int table_index;

            if (table_directory == 0 || (kernel_directory != 0 &&
                pae_entry_physical(&space[index]) == pae_entry_physical(
                    &pae_kernel_pdpt[index]))) {
                continue;
            }
            for (table_index = 0U; table_index < PAE_TABLE_ENTRIES; table_index++) {
                struct pae_entry *pde = &table_directory[table_index];
                struct pae_entry *table;
                unsigned int page_index;

                if (!pae_entry_present(pde)) {
                    continue;
                }
                if (kernel_directory != 0 && pae_entry_present(&kernel_directory[table_index]) &&
                    pae_entry_physical(pde) == pae_entry_physical(&kernel_directory[table_index])) {
                    continue;
                }
                if ((pae_entry_value(pde) & PAE_USER) == 0U) {
                    return 0;
                }
                table = (struct pae_entry *)pae_entry_physical(pde);
                for (page_index = 0U; page_index < PAE_TABLE_ENTRIES; page_index++) {
                    struct pae_entry *pte = &table[page_index];
                    paging_u32_t physical;

                    if (!pae_entry_present(pte)) {
                        continue;
                    }
                    physical = pae_entry_physical(pte);
                    if (!pmm_block_is_user_owned(physical) ||
                        !pmm_user_block_is_mapped(physical)) {
                        return 0;
                    }
                }
            }
        }
        for (index = 0U; index < PAE_PDP_ENTRIES; index++) {
            struct pae_entry *kernel_directory = pae_page_directory_for(
                pae_kernel_pdpt_physical, index);
            struct pae_entry *table_directory = pae_page_directory_for(directory, index);
            unsigned int table_index;

            if (table_directory == 0 || (kernel_directory != 0 &&
                pae_entry_physical(&space[index]) == pae_entry_physical(
                    &pae_kernel_pdpt[index]))) {
                continue;
            }
            for (table_index = 0U; table_index < PAE_TABLE_ENTRIES; table_index++) {
                struct pae_entry *pde = &table_directory[table_index];
                struct pae_entry *table;
                unsigned int page_index;

                if (!pae_entry_present(pde)) {
                    continue;
                }
                if (kernel_directory != 0 && pae_entry_present(&kernel_directory[table_index]) &&
                    pae_entry_physical(pde) == pae_entry_physical(&kernel_directory[table_index])) {
                    continue;
                }
                table = (struct pae_entry *)pae_entry_physical(pde);
                for (page_index = 0U; page_index < PAE_TABLE_ENTRIES; page_index++) {
                    struct pae_entry *pte = &table[page_index];
                    if (pae_entry_present(pte)) {
                        paging_u32_t physical = pae_entry_physical(pte);
                        if ((pae_entry_value(pte) & PAE_USER) != 0U) {
                            pmm_release_user_block(physical);
                        }
                        pmm_free_block(physical);
                    }
                }
                pmm_free_block((paging_u32_t)table);
            }
            pmm_free_block((paging_u32_t)table_directory);
        }
        pmm_free_block(directory);
        for (index = 0U; index < pae_address_space_count; index++) {
            if (pae_address_spaces[index] == directory) {
                pae_address_spaces[index] = pae_address_spaces[pae_address_space_count - 1U];
                pae_address_spaces[pae_address_space_count - 1U] = 0U;
                pae_address_space_count--;
                break;
            }
        }
        return 1;
    }

    if (directory == 0U || directory == kernel_page_directory_physical ||
        directory == page_directory_physical || !paging_address_space_is_valid(directory)) {
        return 0;
    }
    space = (paging_u32_t *)directory;
    /* Preflight all private mappings before freeing anything. A malformed
       page table must never make cleanup release a kernel-owned frame. */
    for (index = 1U; index < (USER_ADDRESS_LIMIT >> 22U); index++) {
        paging_u32_t entry = space[index];
        paging_u32_t *table;
        paging_u32_t table_index;

        if ((entry & PAGE_PRESENT) == 0U ||
            ((kernel_page_directory[index] & PAGE_PRESENT) != 0U &&
             (entry & PAGE_ADDRESS_MASK) ==
                 (kernel_page_directory[index] & PAGE_ADDRESS_MASK))) {
            continue;
        }
        if ((entry & PAGE_FLAG_USER) == 0U) {
            return 0;
        }
        table = (paging_u32_t *)(entry & PAGE_ADDRESS_MASK);
        for (table_index = 0; table_index < PAGE_TABLE_ENTRIES; table_index++) {
            if ((table[table_index] & PAGE_PRESENT) != 0U &&
                (!pmm_block_is_user_owned(table[table_index] & PAGE_ADDRESS_MASK) ||
                 !pmm_user_block_is_mapped(table[table_index] & PAGE_ADDRESS_MASK))) {
                return 0;
            }
        }
    }
    for (index = 1; index < (USER_ADDRESS_LIMIT >> 22U); index++) {
        paging_u32_t entry = space[index];
        paging_u32_t *table;
        paging_u32_t table_index;

        if ((entry & PAGE_PRESENT) == 0U ||
            ((kernel_page_directory[index] & PAGE_PRESENT) != 0U &&
             (entry & PAGE_ADDRESS_MASK) ==
                 (kernel_page_directory[index] & PAGE_ADDRESS_MASK))) {
            continue;
        }
        table = (paging_u32_t *)(entry & PAGE_ADDRESS_MASK);
        for (table_index = 0; table_index < PAGE_TABLE_ENTRIES; table_index++) {
            if ((table[table_index] & PAGE_PRESENT) != 0U) {
                if ((table[table_index] & PAGE_FLAG_USER) != 0U) {
                    pmm_release_user_block(table[table_index] & PAGE_ADDRESS_MASK);
                }
                pmm_free_block(table[table_index] & PAGE_ADDRESS_MASK);
            }
        }
        pmm_free_block((paging_u32_t)table);
    }
    pmm_free_block(directory);
    for (index = 0U; index < address_space_count; index++) {
        if (address_spaces[index] == directory) {
            address_spaces[index] = address_spaces[address_space_count - 1U];
            address_spaces[address_space_count - 1U] = 0U;
            address_space_count--;
            break;
        }
    }
    return 1;
}

static void protect_kernel_read_only(void)
{
    paging_u32_t address = (paging_u32_t)&__text_start;
    paging_u32_t end = ((paging_u32_t)&__rodata_end + PAGE_SIZE - 1U) & PAGE_ADDRESS_MASK;

    if (paging_pae_enabled) {
        paging_u32_t text_end = ((paging_u32_t)&__text_end + PAGE_SIZE - 1U) & PAGE_ADDRESS_MASK;

        while (address < end) {
            struct pae_entry *table = pae_get_page_table(address);
            struct pae_entry *entry = &table[(address >> 12U) & 0x1FFU];
            unsigned long long value = pae_entry_value(entry);

            value &= ~PAE_NX;
            if (address < text_end) {
                value &= ~PAE_WRITABLE;
                value |= (unsigned long long)PAGE_FLAG_EXECUTABLE;
            } else {
                value &= ~(unsigned long long)PAGE_FLAG_EXECUTABLE;
                if (paging_pae_nx_enabled) {
                    value |= PAE_NX;
                }
            }
            pae_set_entry(entry, value);
            address += PAGE_SIZE;
        }
        return;
    }

    while (address < end) {
        paging_u32_t *table = get_page_table(address);
        paging_u32_t index = (address >> 12U) & 0x3FFU;

        table[index] &= ~PAGE_FLAG_WRITABLE;
        address += PAGE_SIZE;
    }
}

static void enable_paging(void)
{
    paging_u32_t cr0;

    if (paging_pae_enabled) {
        paging_u32_t cr4;

        __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
        cr4 |= CR4_PAE;
        __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4) : "memory");
        if (paging_pae_nx_enabled) {
            paging_u32_t efer_low;
            paging_u32_t efer_high;

            __asm__ volatile ("rdmsr" : "=a"(efer_low), "=d"(efer_high) : "c"(EFER_MSR));
            efer_low |= EFER_NXE;
            __asm__ volatile ("wrmsr" : : "a"(efer_low), "d"(efer_high), "c"(EFER_MSR));
        }
    }

    __asm__ volatile ("mov %0, %%cr3" : : "r"(page_directory_physical) : "memory");
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= PAGE_ENABLE | PAGE_WRITE_PROTECT;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
    paging_enabled = 1;
}

static int pae_init(const struct boot_info *boot_info)
{
    paging_u32_t pdpt_physical = pmm_alloc_block_below(LOW_IDENTITY_LIMIT);
    paging_u32_t page_directory_physical_local = pmm_alloc_block_below(LOW_IDENTITY_LIMIT);
    paging_u32_t identity_table_physical = pmm_alloc_block_below(LOW_IDENTITY_LIMIT);
    paging_u32_t identity_table_second_physical = pmm_alloc_block_below(LOW_IDENTITY_LIMIT);
    struct pae_entry *page_directory;
    struct pae_entry *identity_table;
    struct pae_entry *identity_table_second;
    unsigned int index;

    if (pdpt_physical == 0U || page_directory_physical_local == 0U ||
        identity_table_physical == 0U || identity_table_second_physical == 0U) {
        if (pdpt_physical != 0U) {
            pmm_free_block(pdpt_physical);
        }
        if (page_directory_physical_local != 0U) {
            pmm_free_block(page_directory_physical_local);
        }
        if (identity_table_physical != 0U) {
            pmm_free_block(identity_table_physical);
        }
        if (identity_table_second_physical != 0U) {
            pmm_free_block(identity_table_second_physical);
        }
        return 0;
    }

    pae_pdpt_physical = pdpt_physical;
    pae_kernel_pdpt_physical = pdpt_physical;
    pae_pdpt = (struct pae_entry *)pdpt_physical;
    pae_kernel_pdpt = pae_pdpt;
    page_directory_physical = pdpt_physical;
    kernel_page_directory_physical = pdpt_physical;
    page_directory = (struct pae_entry *)page_directory_physical_local;
    identity_table = (struct pae_entry *)identity_table_physical;
    identity_table_second = (struct pae_entry *)identity_table_second_physical;
    clear_pae_page(pae_pdpt);
    clear_pae_page(page_directory);
    clear_pae_page(identity_table);
    clear_pae_page(identity_table_second);
    pae_set_entry(&pae_pdpt[0], (unsigned long long)page_directory_physical_local |
        PAE_PRESENT);
    pae_set_entry(&page_directory[0], (unsigned long long)identity_table_physical |
        PAE_PRESENT | PAE_WRITABLE);
    pae_set_entry(&page_directory[1], (unsigned long long)identity_table_second_physical |
        PAE_PRESENT | PAE_WRITABLE);
    for (index = 1U; index < PAE_TABLE_ENTRIES; index++) {
        pae_set_entry(&identity_table[index], (unsigned long long)(index * PAGE_SIZE) |
            PAE_PRESENT | PAE_WRITABLE | (paging_pae_nx_enabled ? PAE_NX : 0U));
    }
    for (index = 0U; index < PAE_TABLE_ENTRIES; index++) {
        pae_set_entry(&identity_table_second[index],
            (unsigned long long)((PAE_TABLE_ENTRIES + index) * PAGE_SIZE) |
            PAE_PRESENT | PAE_WRITABLE | (paging_pae_nx_enabled ? PAE_NX : 0U));
    }
    pae_address_space_count = 0U;
    /* The legacy pointer is intentionally unused in PAE mode. */
    kernel_page_directory = 0;
    if (boot_info != 0 && (boot_info->video_flags & BOOT_VIDEO_FONT_AVAILABLE) != 0U) {
        for (index = 0U; index < VBE_FRAMEBUFFER_PAGES; index++) {
            paging_u32_t address = VBE_FRAMEBUFFER_VIRTUAL + (index * PAGE_SIZE);
            if (!paging_map_page(address, address, PAGE_FLAG_WRITABLE)) {
                return 0;
            }
        }
    }
    protect_kernel_read_only();
    enable_paging();
    return 1;
}

int paging_init(const struct boot_info *boot_info)
{
    paging_u32_t identity_table_physical;
    paging_u32_t *identity_table;
    paging_u32_t index;

    paging_pae_enabled = 0;
    paging_pae_nx_enabled = 0;
    pae_pdpt = 0;
    pae_kernel_pdpt = 0;
    pae_pdpt_physical = 0U;
    pae_kernel_pdpt_physical = 0U;
    pae_address_space_count = 0U;
    {
        const struct cpu_features *features = cpu_features_get();
        if (features->pae != 0U) {
            paging_pae_enabled = 1;
            paging_pae_nx_enabled = features->nx != 0U;
            return pae_init(boot_info);
        }
    }

    page_directory_physical = pmm_alloc_block_below(LOW_IDENTITY_LIMIT);
    identity_table_physical = pmm_alloc_block_below(LOW_IDENTITY_LIMIT);
    if (page_directory_physical == 0U || identity_table_physical == 0U) {
        if (page_directory_physical != 0U) {
            pmm_free_block(page_directory_physical);
        }
        if (identity_table_physical != 0U) {
            pmm_free_block(identity_table_physical);
        }
        page_directory_physical = 0;
        return 0;
    }

    page_directory = (paging_u32_t *)page_directory_physical;
    kernel_page_directory = page_directory;
    kernel_page_directory_physical = page_directory_physical;
    address_space_count = 0U;
    identity_table = (paging_u32_t *)identity_table_physical;
    clear_page(page_directory);
    clear_page(identity_table);

    for (index = 1; index < PAGE_TABLE_ENTRIES; index++) {
        identity_table[index] = (index * PAGE_SIZE) | PAGE_PRESENT | PAGE_FLAG_WRITABLE;
    }
    page_directory[0] = identity_table_physical | PAGE_PRESENT | PAGE_FLAG_WRITABLE;

    if (boot_info != 0 && (boot_info->video_flags & BOOT_VIDEO_FONT_AVAILABLE) != 0U) {
        for (index = 0; index < VBE_FRAMEBUFFER_PAGES; index++) {
            paging_u32_t address = VBE_FRAMEBUFFER_VIRTUAL + (index * PAGE_SIZE);
            if (!paging_map_page(address, address, PAGE_FLAG_WRITABLE)) {
                while (index != 0U) {
                    index--;
                    paging_unmap_page(VBE_FRAMEBUFFER_VIRTUAL + (index * PAGE_SIZE));
                }
                pmm_free_block(identity_table_physical);
                pmm_free_block(page_directory_physical);
                page_directory = 0;
                page_directory_physical = 0;
                return 0;
            }
        }
    }

    protect_kernel_read_only();
    enable_paging();
    return 1;
}

static int mixed_user_table_self_test(void)
{
    const paging_u32_t supervisor_virtual = 0x01000000U;
    const paging_u32_t user_virtual = supervisor_virtual + PAGE_SIZE;
    paging_u32_t kernel_directory = paging_kernel_directory();
    paging_u32_t directory = paging_create_address_space();
    paging_u32_t supervisor_frame = 0U;
    paging_u32_t user_frame = 0U;
    int supervisor_mapped = 0;
    int user_mapped = 0;
    int result = 0;


    if (directory == 0U || !paging_switch_address_space(directory)) {
        return 0;
    }
    supervisor_frame = pmm_alloc_block();
    user_frame = pmm_alloc_user_block();
    if (supervisor_frame == 0U || user_frame == 0U ||
        !paging_map_page(supervisor_virtual, supervisor_frame, PAGE_FLAG_WRITABLE)) {
        goto cleanup;
    }
    supervisor_mapped = 1;
    if (paging_map_page(user_virtual, user_frame, PAGE_FLAG_USER | PAGE_FLAG_WRITABLE)) {
        user_mapped = 1;
        goto cleanup;
    }
    result = 1;

cleanup:
    if (user_mapped) {
        if (paging_unmap_page(user_virtual) != user_frame) {
            result = 0;
        }
    }
    if (supervisor_mapped) {
        if (paging_unmap_page(supervisor_virtual) != supervisor_frame) {
            result = 0;
        }
    }
    if (supervisor_frame != 0U) {
        pmm_free_block(supervisor_frame);
    }
    if (user_frame != 0U) {
        pmm_free_block(user_frame);
    }
    if (!paging_switch_address_space(kernel_directory) ||
        !paging_destroy_address_space(directory)) {
        return 0;
    }
    return result;
}

static int pae_self_test(void)
{
    const paging_u32_t test_virtual = 0xCFF00000U;
    const paging_u32_t shared_virtual = 0x00800000U;
    paging_u32_t text_address = (paging_u32_t)&__text_start;
    struct pae_entry *text_table = pae_get_page_table(text_address);
    struct pae_entry *text_entry;
    paging_u32_t free_before = pmm_free_blocks();
    paging_u32_t frame = 0U;
    paging_u32_t shared_frame = 0U;
    paging_u32_t user_frame = 0U;
    paging_u32_t unmapped;
    paging_u32_t cr0;
    volatile paging_u32_t *test_pointer;
    int mixed_result;

    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    if (paging_is_mapped(0U) || paging_is_mapped(test_virtual) || text_table == 0 ||
        paging_switch_address_space(0x00400000U) || !paging_is_mapped(PAGE_SIZE) ||
        paging_unmap_page(PAGE_SIZE) != 0U || !paging_is_mapped(PAGE_SIZE) ||
        (cr0 & PAGE_WRITE_PROTECT) == 0U) {
        return 0;
    }
    text_entry = &text_table[(text_address >> 12U) & 0x1FFU];
    if ((pae_entry_value(text_entry) & (PAE_WRITABLE | PAGE_FLAG_EXECUTABLE)) !=
        PAGE_FLAG_EXECUTABLE || (paging_pae_nx_enabled &&
        (pae_entry_value(text_entry) & PAE_NX) != 0U)) {
        return 0;
    }

    user_frame = pmm_alloc_block();
    if (user_frame == 0U || paging_map_page(PAGE_SIZE, user_frame,
        PAGE_FLAG_USER | PAGE_FLAG_WRITABLE) || paging_map_page(PAGE_SIZE, user_frame, 0x008U)) {
        if (user_frame != 0U) {
            pmm_free_block(user_frame);
        }
        return 0;
    }
    pmm_free_block(user_frame);

    shared_frame = pmm_alloc_block();
    if (shared_frame == 0U || !paging_map_page(shared_virtual, shared_frame, PAGE_FLAG_WRITABLE)) {
        if (shared_frame != 0U) {
            pmm_free_block(shared_frame);
        }
        return 0;
    }
    if (paging_protect_page(shared_virtual, PAGE_FLAG_USER | PAGE_FLAG_WRITABLE) ||
        paging_map_page(shared_virtual + PAGE_SIZE, shared_frame,
            PAGE_FLAG_USER | PAGE_FLAG_WRITABLE) ||
        paging_unmap_page(shared_virtual) != shared_frame) {
        if (paging_is_mapped(shared_virtual + PAGE_SIZE)) {
            paging_unmap_page(shared_virtual + PAGE_SIZE);
        }
        if (paging_is_mapped(shared_virtual)) {
            paging_unmap_page(shared_virtual);
        }
        pmm_free_block(shared_frame);
        return 0;
    }
    pmm_free_block(shared_frame);

    frame = pmm_alloc_block();
    if (frame == 0U || paging_map_page(test_virtual, 0U, PAGE_FLAG_WRITABLE) ||
        paging_map_page(test_virtual, frame, 0x008U) ||
        paging_map_page(test_virtual, frame, PAGE_FLAG_WRITABLE | PAGE_FLAG_EXECUTABLE) ||
        !paging_map_page(test_virtual, frame, PAGE_FLAG_WRITABLE)) {
        if (frame != 0U) {
            pmm_free_block(frame);
        }
        return 0;
    }
    if (paging_protect_page(test_virtual, PAGE_FLAG_WRITABLE | PAGE_FLAG_EXECUTABLE)) {
        paging_unmap_page(test_virtual);
        pmm_free_block(frame);
        return 0;
    }
    if (paging_pae_nx_enabled) {
        struct pae_entry *test_table = pae_get_page_table(test_virtual);
        if (test_table == 0 || (pae_entry_value(
            &test_table[(test_virtual >> 12U) & 0x1FFU]) & PAE_NX) == 0U) {
            paging_unmap_page(test_virtual);
            pmm_free_block(frame);
            return 0;
        }
    }
    test_pointer = (volatile paging_u32_t *)test_virtual;
    *test_pointer = 0xEF05A55AU;
    if (*test_pointer != 0xEF05A55AU || paging_get_physical(test_virtual) != frame) {
        paging_unmap_page(test_virtual);
        pmm_free_block(frame);
        return 0;
    }
    unmapped = paging_unmap_page(test_virtual);
    pmm_free_block(frame);
    mixed_result = mixed_user_table_self_test();
    return unmapped == frame && !paging_is_mapped(test_virtual) &&
        mixed_result && pmm_free_blocks() == free_before;
}

int paging_self_test(void)
{
    const paging_u32_t test_virtual = 0xCFF00000U;
    paging_u32_t text_address = (paging_u32_t)&__text_start;
    paging_u32_t *text_table = get_page_table(text_address);
    paging_u32_t free_before = pmm_free_blocks();
    paging_u32_t frame;
    paging_u32_t unmapped;
    paging_u32_t user_frame;
    paging_u32_t shared_frame;
    const paging_u32_t shared_virtual = 0x00800000U;
    paging_u32_t cr0;
    volatile paging_u32_t *test_pointer;

    if (paging_pae_enabled) {
        return pae_self_test();
    }

    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    if (paging_is_mapped(0U) || paging_is_mapped(test_virtual) || text_table == 0 ||
        paging_switch_address_space(0x00400000U) ||
        !paging_is_mapped(PAGE_SIZE) || paging_unmap_page(PAGE_SIZE) != 0U ||
        !paging_is_mapped(PAGE_SIZE) ||
        (text_table[(text_address >> 12U) & 0x3FFU] & PAGE_FLAG_WRITABLE) != 0U ||
        (cr0 & PAGE_WRITE_PROTECT) == 0U) {
        return 0;
    }

    user_frame = pmm_alloc_block();
    if (user_frame == 0U || paging_map_page(PAGE_SIZE, user_frame,
        PAGE_FLAG_USER | PAGE_FLAG_WRITABLE) ||
        paging_map_page(PAGE_SIZE, user_frame, 0x008U)) {
        if (user_frame != 0U) {
            if (paging_is_mapped(PAGE_SIZE)) {
                paging_unmap_page(PAGE_SIZE);
            }
            pmm_free_block(user_frame);
        }
        return 0;
    }
    pmm_free_block(user_frame);

    shared_frame = pmm_alloc_block();
    if (shared_frame == 0U ||
        !paging_map_page(shared_virtual, shared_frame, PAGE_FLAG_WRITABLE)) {
        if (paging_is_mapped(shared_virtual + PAGE_SIZE)) {
            paging_unmap_page(shared_virtual + PAGE_SIZE);
        }
        if (paging_is_mapped(shared_virtual)) {
            paging_unmap_page(shared_virtual);
        }
        if (shared_frame != 0U) {
            pmm_free_block(shared_frame);
        }
        return 0;
    }
    if (paging_protect_page(shared_virtual, PAGE_FLAG_USER | PAGE_FLAG_WRITABLE)) {
        paging_unmap_page(shared_virtual);
        pmm_free_block(shared_frame);
        return 0;
    }
    if (paging_map_page(shared_virtual + PAGE_SIZE, shared_frame,
        PAGE_FLAG_USER | PAGE_FLAG_WRITABLE)) {
        paging_unmap_page(shared_virtual + PAGE_SIZE);
        paging_unmap_page(shared_virtual);
        pmm_free_block(shared_frame);
        return 0;
    }
    if (paging_unmap_page(shared_virtual) != shared_frame) {
        pmm_free_block(shared_frame);
        return 0;
    }
    pmm_free_block(shared_frame);

    frame = pmm_alloc_block();
    if (frame == 0U || paging_map_page(test_virtual, 0U, PAGE_FLAG_WRITABLE) ||
        paging_map_page(test_virtual, frame, 0x008U) ||
        paging_map_page(test_virtual, frame, PAGE_FLAG_WRITABLE | PAGE_FLAG_EXECUTABLE) ||
        !paging_map_page(test_virtual, frame, PAGE_FLAG_WRITABLE)) {
        if (frame != 0U) {
            pmm_free_block(frame);
        }
        return 0;
    }

    if (paging_protect_page(test_virtual, PAGE_FLAG_WRITABLE | PAGE_FLAG_EXECUTABLE)) {
        paging_unmap_page(test_virtual);
        pmm_free_block(frame);
        return 0;
    }

    test_pointer = (volatile paging_u32_t *)test_virtual;
    *test_pointer = 0xEF05A55AU;
    if (*test_pointer != 0xEF05A55AU || paging_get_physical(test_virtual) != frame) {
        paging_unmap_page(test_virtual);
        pmm_free_block(frame);
        return 0;
    }

    unmapped = paging_unmap_page(test_virtual);
    pmm_free_block(frame);
    return unmapped == frame && !paging_is_mapped(test_virtual) &&
        mixed_user_table_self_test() && pmm_free_blocks() == free_before;
}
