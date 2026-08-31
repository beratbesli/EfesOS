#include "heap.h"
#include "paging.h"
#include "panic.h"
#include "pmm.h"

#define HEAP_START 0xD0000000U
#define HEAP_MAX_SIZE (16U * 1024U * 1024U)
#define HEAP_ALIGNMENT 16U
#define HEAP_BLOCK_MAGIC 0x48454150U
#define HEAP_COOKIE_SEED 0xEF05C0DEU
#define HEAP_BLOCK_FREE 1U

struct heap_block {
    unsigned int magic;
    unsigned int capacity;
    unsigned int requested;
    unsigned int cookie;
    struct heap_block *previous;
    struct heap_block *next;
    unsigned int flags;
    unsigned int reserved;
};

static struct heap_block *first_block;
static unsigned int mapped_bytes;
static unsigned int used_bytes;
static int initialized;

static unsigned int interrupt_save(void)
{
    unsigned int flags;

    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void interrupt_restore(unsigned int flags)
{
    __asm__ volatile ("pushl %0; popfl" : : "r"(flags) : "memory", "cc");
}

static unsigned int align_up(unsigned int value)
{
    return (value + HEAP_ALIGNMENT - 1U) & ~(HEAP_ALIGNMENT - 1U);
}

static unsigned int block_cookie(const struct heap_block *block)
{
    return HEAP_COOKIE_SEED ^ (unsigned int)block ^ block->capacity ^ block->requested;
}

static void refresh_block_cookie(struct heap_block *block)
{
    block->cookie = block_cookie(block);
}

static int block_is_valid(const struct heap_block *block)
{
    unsigned int address = (unsigned int)block;
    unsigned int heap_end = HEAP_START + mapped_bytes;

    return address >= HEAP_START &&
           address <= heap_end - sizeof(*block) &&
           block->magic == HEAP_BLOCK_MAGIC &&
           block->capacity <= heap_end - address - sizeof(*block) &&
           block->cookie == block_cookie(block);
}

static unsigned int *block_canary(struct heap_block *block)
{
    return (unsigned int *)((unsigned char *)(block + 1) + block->requested);
}

static void write_canary(struct heap_block *block)
{
    *block_canary(block) = block->cookie;
}

static int canary_is_valid(struct heap_block *block)
{
    return *block_canary(block) == block->cookie;
}

static void zero_page(unsigned int virtual_address)
{
    unsigned int *words = (unsigned int *)virtual_address;
    unsigned int index;

    for (index = 0; index < PAGE_SIZE / sizeof(unsigned int); index++) {
        words[index] = 0U;
    }
}

static void rollback_pages(unsigned int start_offset, unsigned int page_count)
{
    while (page_count != 0U) {
        unsigned int virtual_address;
        unsigned int physical_address;

        page_count--;
        virtual_address = HEAP_START + start_offset + (page_count * PAGE_SIZE);
        physical_address = paging_unmap_page(virtual_address);
        if (physical_address != 0U) {
            pmm_free_block(physical_address);
        }
    }
}

static int expand_heap(unsigned int minimum_bytes)
{
    unsigned int page_count;
    unsigned int added_bytes;
    unsigned int mapped_pages = 0;
    unsigned int old_mapped_bytes = mapped_bytes;
    struct heap_block *last;

    if (minimum_bytes == 0U || minimum_bytes > HEAP_MAX_SIZE - mapped_bytes) {
        return 0;
    }
    page_count = (minimum_bytes + PAGE_SIZE - 1U) / PAGE_SIZE;
    added_bytes = page_count * PAGE_SIZE;
    if (added_bytes > HEAP_MAX_SIZE - mapped_bytes) {
        return 0;
    }

    while (mapped_pages < page_count) {
        unsigned int physical_address = pmm_alloc_block();
        unsigned int virtual_address = HEAP_START + old_mapped_bytes + (mapped_pages * PAGE_SIZE);

        if (physical_address == 0U ||
            !paging_map_page(virtual_address, physical_address, PAGE_FLAG_WRITABLE)) {
            if (physical_address != 0U) {
                pmm_free_block(physical_address);
            }
            rollback_pages(old_mapped_bytes, mapped_pages);
            return 0;
        }
        zero_page(virtual_address);
        mapped_pages++;
    }

    mapped_bytes += added_bytes;
    if (first_block == 0) {
        first_block = (struct heap_block *)HEAP_START;
        first_block->magic = HEAP_BLOCK_MAGIC;
        first_block->capacity = added_bytes - sizeof(*first_block);
        first_block->requested = 0;
        first_block->previous = 0;
        first_block->next = 0;
        first_block->flags = HEAP_BLOCK_FREE;
        first_block->reserved = 0;
        refresh_block_cookie(first_block);
        return 1;
    }

    last = first_block;
    for (;;) {
        if (!block_is_valid(last)) {
            kernel_panic("Kernel heap metadata corruption.");
        }
        if (last->next == 0) {
            break;
        }
        if ((unsigned int)last->next <= (unsigned int)last) {
            kernel_panic("Kernel heap list order corruption.");
        }
        last = last->next;
    }

    if ((last->flags & HEAP_BLOCK_FREE) != 0U) {
        last->capacity += added_bytes;
        refresh_block_cookie(last);
    } else {
        struct heap_block *new_block = (struct heap_block *)(HEAP_START + old_mapped_bytes);

        new_block->magic = HEAP_BLOCK_MAGIC;
        new_block->capacity = added_bytes - sizeof(*new_block);
        new_block->requested = 0;
        new_block->previous = last;
        new_block->next = 0;
        new_block->flags = HEAP_BLOCK_FREE;
        new_block->reserved = 0;
        refresh_block_cookie(new_block);
        last->next = new_block;
        refresh_block_cookie(last);
    }
    return 1;
}

static struct heap_block *find_free_block(unsigned int capacity)
{
    struct heap_block *block = first_block;

    while (block != 0) {
        if (!block_is_valid(block)) {
            kernel_panic("Kernel heap list corruption.");
        }
        if ((block->flags & HEAP_BLOCK_FREE) != 0U && block->capacity >= capacity) {
            return block;
        }
        block = block->next;
    }
    return 0;
}

static void split_block(struct heap_block *block, unsigned int capacity)
{
    struct heap_block *remainder;

    if (block->capacity < capacity + sizeof(*block) + HEAP_ALIGNMENT) {
        return;
    }

    remainder = (struct heap_block *)((unsigned char *)(block + 1) + capacity);
    remainder->magic = HEAP_BLOCK_MAGIC;
    remainder->capacity = block->capacity - capacity - sizeof(*block);
    remainder->requested = 0;
    remainder->previous = block;
    remainder->next = block->next;
    remainder->flags = HEAP_BLOCK_FREE;
    remainder->reserved = 0;
    if (remainder->next != 0) {
        remainder->next->previous = remainder;
        refresh_block_cookie(remainder->next);
    }
    refresh_block_cookie(remainder);

    block->capacity = capacity;
    block->next = remainder;
    refresh_block_cookie(block);
}

static void merge_with_next(struct heap_block *block)
{
    struct heap_block *next = block->next;

    if (next == 0) {
        return;
    }
    if (!block_is_valid(next)) {
        kernel_panic("Kernel heap next-block corruption.");
    }
    if ((next->flags & HEAP_BLOCK_FREE) == 0U) {
        return;
    }
    if ((unsigned char *)(block + 1) + block->capacity != (unsigned char *)next) {
        kernel_panic("Kernel heap adjacency corruption.");
    }

    block->capacity += sizeof(*next) + next->capacity;
    block->next = next->next;
    if (block->next != 0) {
        block->next->previous = block;
        refresh_block_cookie(block->next);
    }
    refresh_block_cookie(block);
}

int heap_init(void)
{
    unsigned int flags;
    int result;

    if (initialized) {
        return 1;
    }
    flags = interrupt_save();
    result = expand_heap(PAGE_SIZE);
    if (result) {
        initialized = 1;
    }
    interrupt_restore(flags);
    return result;
}

void *kmalloc(unsigned int size)
{
    unsigned int flags;
    unsigned int required_capacity;
    struct heap_block *block;

    if (!initialized || size == 0U || size > HEAP_MAX_SIZE - sizeof(unsigned int) - HEAP_ALIGNMENT) {
        return 0;
    }
    required_capacity = align_up(size + sizeof(unsigned int));
    flags = interrupt_save();
    block = find_free_block(required_capacity);
    if (block == 0 && expand_heap(required_capacity + sizeof(*block))) {
        block = find_free_block(required_capacity);
    }
    if (block == 0) {
        interrupt_restore(flags);
        return 0;
    }

    split_block(block, required_capacity);
    block->requested = size;
    block->flags &= ~HEAP_BLOCK_FREE;
    used_bytes += size;
    refresh_block_cookie(block);
    write_canary(block);
    interrupt_restore(flags);
    return block + 1;
}

void *kcalloc(unsigned int count, unsigned int size)
{
    unsigned int total;
    unsigned int index;
    unsigned char *memory;

    if (count != 0U && size > 0xFFFFFFFFU / count) {
        return 0;
    }
    total = count * size;
    memory = (unsigned char *)kmalloc(total);
    if (memory == 0) {
        return 0;
    }
    for (index = 0; index < total; index++) {
        memory[index] = 0;
    }
    return memory;
}

void kfree(void *pointer)
{
    unsigned int flags;
    unsigned int address = (unsigned int)pointer;
    struct heap_block *block;

    if (pointer == 0) {
        return;
    }
    flags = interrupt_save();
    if (address < HEAP_START + sizeof(struct heap_block) || address >= HEAP_START + mapped_bytes ||
        (address & (HEAP_ALIGNMENT - 1U)) != 0U) {
        kernel_panic("Invalid kernel heap free.");
    }

    block = ((struct heap_block *)pointer) - 1;
    if (!block_is_valid(block) || (block->flags & HEAP_BLOCK_FREE) != 0U ||
        block->capacity < sizeof(unsigned int) ||
        block->requested > block->capacity - sizeof(unsigned int) || !canary_is_valid(block)) {
        kernel_panic("Kernel heap overrun or double free.");
    }

    used_bytes -= block->requested;
    block->requested = 0;
    block->flags |= HEAP_BLOCK_FREE;
    refresh_block_cookie(block);
    merge_with_next(block);
    if (block->previous != 0) {
        if (!block_is_valid(block->previous)) {
            kernel_panic("Kernel heap previous-block corruption.");
        }
        if ((block->previous->flags & HEAP_BLOCK_FREE) != 0U) {
            block = block->previous;
            merge_with_next(block);
        }
    }
    interrupt_restore(flags);
}

unsigned int heap_mapped_bytes(void)
{
    return mapped_bytes;
}

unsigned int heap_used_bytes(void)
{
    return used_bytes;
}

int heap_self_test(void)
{
    unsigned char *small = (unsigned char *)kmalloc(31U);
    unsigned char *large = (unsigned char *)kmalloc(8192U);
    unsigned int *zeroed = (unsigned int *)kcalloc(64U, sizeof(unsigned int));
    unsigned int index;
    void *coalesced;

    if (small == 0 || large == 0 || zeroed == 0 ||
        ((unsigned int)small & (HEAP_ALIGNMENT - 1U)) != 0U ||
        ((unsigned int)large & (HEAP_ALIGNMENT - 1U)) != 0U) {
        return 0;
    }
    for (index = 0; index < 64U; index++) {
        if (zeroed[index] != 0U) {
            return 0;
        }
    }
    for (index = 0; index < 31U; index++) {
        small[index] = (unsigned char)(index ^ 0xA5U);
    }
    for (index = 0; index < 8192U; index++) {
        large[index] = (unsigned char)(index ^ 0x5AU);
    }

    kfree(large);
    kfree(small);
    kfree(zeroed);
    coalesced = kmalloc(9000U);
    if (coalesced == 0) {
        return 0;
    }
    kfree(coalesced);
    return heap_used_bytes() == 0U;
}
