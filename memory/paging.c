#include "paging.h"
#include "pmm.h"

#define PAGE_TABLE_ENTRIES 1024U
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

extern unsigned char __text_start;
extern unsigned char __rodata_end;

static paging_u32_t *page_directory;
static paging_u32_t page_directory_physical;
static paging_u32_t *kernel_page_directory;
static paging_u32_t kernel_page_directory_physical;
static paging_u32_t address_spaces[PAGING_MAX_ADDRESS_SPACES];
static unsigned int address_space_count;
static int paging_enabled;

static void clear_page(paging_u32_t *page)
{
    paging_u32_t index;

    for (index = 0; index < PAGE_TABLE_ENTRIES; index++) {
        page[index] = 0;
    }
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

    if (table == 0) {
        return 0;
    }
    return (table[(virtual_address >> 12U) & 0x3FFU] & PAGE_PRESENT) != 0U;
}

int paging_validate_user_range(paging_u32_t virtual_address, paging_u32_t length, int writable)
{
    paging_u32_t end;
    paging_u32_t page;

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

paging_u32_t paging_create_address_space(void)
{
    paging_u32_t physical;
    paging_u32_t *directory;
    paging_u32_t index;

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
    return directory != 0U && (directory & (PAGE_SIZE - 1U)) == 0U &&
        address_space_is_registered(directory);
}

int paging_switch_address_space(paging_u32_t directory)
{
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

    __asm__ volatile ("mov %0, %%cr3" : : "r"(page_directory_physical) : "memory");
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= PAGE_ENABLE | PAGE_WRITE_PROTECT;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
    paging_enabled = 1;
}

int paging_init(const struct boot_info *boot_info)
{
    paging_u32_t identity_table_physical;
    paging_u32_t *identity_table;
    paging_u32_t index;

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
    return unmapped == frame && !paging_is_mapped(test_virtual) && pmm_free_blocks() == free_before;
}
