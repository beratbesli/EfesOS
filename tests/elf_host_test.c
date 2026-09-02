#include <stdio.h>
#include <stdlib.h>

#include "elf_loader.h"
#include "paging.h"
#include "pmm.h"

__attribute__((noreturn)) void kernel_panic(const char *message)
{
    (void)message;
    abort();
}

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

int paging_validate_user_execute(paging_u32_t virtual_address)
{
    (void)virtual_address;
    return 0;
}

paging_u32_t paging_kernel_directory(void)
{
    return 0U;
}

paging_u32_t paging_current_directory(void)
{
    return 0U;
}

paging_u32_t paging_create_address_space(void)
{
    return 0U;
}

int paging_address_space_is_valid(paging_u32_t directory)
{
    (void)directory;
    return 0;
}

int paging_switch_address_space(paging_u32_t directory)
{
    (void)directory;
    return 0;
}

int paging_destroy_address_space(paging_u32_t directory)
{
    (void)directory;
    return 0;
}

static void set_u16(unsigned char *data, unsigned int offset, unsigned int value)
{
    data[offset] = (unsigned char)(value & 0xFFU);
    data[offset + 1U] = (unsigned char)(value >> 8U);
}

static void set_u32(unsigned char *data, unsigned int offset, unsigned int value)
{
    data[offset] = (unsigned char)(value & 0xFFU);
    data[offset + 1U] = (unsigned char)((value >> 8U) & 0xFFU);
    data[offset + 2U] = (unsigned char)((value >> 16U) & 0xFFU);
    data[offset + 3U] = (unsigned char)(value >> 24U);
}

static void build_fixture(unsigned char *image, unsigned int size)
{
    unsigned int index;

    for (index = 0U; index < size; index++) {
        image[index] = 0U;
    }
    image[0] = 0x7FU;
    image[1] = 'E';
    image[2] = 'L';
    image[3] = 'F';
    image[4] = 1U;
    image[5] = 1U;
    image[6] = 1U;
    set_u16(image, 16U, 2U);
    set_u16(image, 18U, 3U);
    set_u32(image, 20U, 1U);
    set_u32(image, 24U, 0x00400000U);
    set_u32(image, 28U, 52U);
    set_u16(image, 40U, 52U);
    set_u16(image, 42U, 32U);
    set_u16(image, 44U, 1U);
    set_u32(image, 52U, 1U);
    set_u32(image, 56U, 116U);
    set_u32(image, 60U, 0x00400000U);
    set_u32(image, 68U, 4U);
    set_u32(image, 72U, 4096U);
    set_u32(image, 76U, 1U);
    set_u32(image, 80U, 1U);
    image[116] = 0xC3U;
}

static int mutation_scan(void)
{
    unsigned char image[256];
    unsigned int offset;
    unsigned int value;
    unsigned int size;

    build_fixture(image, sizeof(image));
    if (!elf_validate_image(image, sizeof(image), 0)) {
        return 0;
    }

    /* Exhaustively vary every byte. The parser must remain bounded even when
       a mutation changes an offset, count, size, flag, or address field. */
    for (offset = 0U; offset < sizeof(image); offset++) {
        unsigned char original = image[offset];
        for (value = 0U; value < 256U; value++) {
            image[offset] = (unsigned char)value;
            (void)elf_validate_image(image, sizeof(image), 0);
        }
        image[offset] = original;
    }

    /* Truncation is a separate boundary condition from byte corruption. */
    for (size = 0U; size <= sizeof(image); size++) {
        (void)elf_validate_image(image, size, 0);
    }
    return 1;
}

int main(void)
{
    if (!elf_loader_self_test() || !mutation_scan()) {
        return 1;
    }
    puts("ELF host self-test passed.");
    return 0;
}
