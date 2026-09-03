#include "acpi.h"
#include "acpi_tables.h"
#include "paging.h"

#define ACPI_MAP_VIRTUAL 0xCF000000U
#define ACPI_MAP_MAX_PAGES 17U
#define ACPI_MAP_MAX_BYTES (ACPI_MAP_MAX_PAGES * PAGE_SIZE)

struct acpi_mapped_region {
    const unsigned char *bytes;
    unsigned int physical_page;
    unsigned int page_count;
};

static int initialized;
static int available;
static int hpet_available;
static struct acpi_hpet_info hpet_info;
static unsigned int rsdt_entries[ACPI_MAX_RSDT_ENTRIES];

static int signature_equal(const unsigned char *bytes, const char signature[4])
{
    return bytes != 0 && signature != 0 &&
        bytes[0] == (unsigned char)signature[0] &&
        bytes[1] == (unsigned char)signature[1] &&
        bytes[2] == (unsigned char)signature[2] &&
        bytes[3] == (unsigned char)signature[3];
}

static int unmap_region(struct acpi_mapped_region *region)
{
    unsigned int index;
    int result = 1;

    if (region == 0) {
        return 0;
    }
    for (index = 0U; index < region->page_count; index++) {
        unsigned int expected = region->physical_page + index * PAGE_SIZE;
        unsigned int unmapped = paging_unmap_page(
            ACPI_MAP_VIRTUAL + index * PAGE_SIZE);

        if (unmapped != expected) {
            result = 0;
        }
    }
    region->bytes = 0;
    region->physical_page = 0U;
    region->page_count = 0U;
    return result;
}

static int map_region(unsigned int physical_address, unsigned int length,
    struct acpi_mapped_region *region)
{
    unsigned int physical_page;
    unsigned int page_offset;
    unsigned int total;
    unsigned int pages;
    unsigned int index;

    if (region == 0 || length == 0U || physical_address < PAGE_SIZE ||
        physical_address > 0xFFFFFFFFU - (length - 1U)) {
        return 0;
    }
    physical_page = physical_address & ~(PAGE_SIZE - 1U);
    page_offset = physical_address & (PAGE_SIZE - 1U);
    if (length > ACPI_MAP_MAX_BYTES - page_offset) {
        return 0;
    }
    total = page_offset + length;
    pages = (total + PAGE_SIZE - 1U) / PAGE_SIZE;
    if (pages == 0U || pages > ACPI_MAP_MAX_PAGES) {
        return 0;
    }
    region->bytes = 0;
    region->physical_page = physical_page;
    region->page_count = 0U;
    for (index = 0U; index < pages; index++) {
        unsigned int virtual_address = ACPI_MAP_VIRTUAL + index * PAGE_SIZE;
        unsigned int frame = physical_page + index * PAGE_SIZE;

        if (frame < physical_page || paging_is_mapped(virtual_address) ||
            !paging_map_page(virtual_address, frame, 0U)) {
            unmap_region(region);
            return 0;
        }
        region->page_count++;
    }
    region->bytes = (const unsigned char *)(ACPI_MAP_VIRTUAL + page_offset);
    return 1;
}

static int map_sdt(unsigned int physical_address, const char signature[4],
    struct acpi_mapped_region *region, unsigned int *length)
{
    struct acpi_mapped_region header;
    unsigned int table_length;

    if (!map_region(physical_address, ACPI_SDT_HEADER_LENGTH, &header)) {
        return 0;
    }
    if (!acpi_sdt_peek_length(header.bytes, ACPI_SDT_HEADER_LENGTH,
            signature, &table_length)) {
        unmap_region(&header);
        return 0;
    }
    if (!unmap_region(&header) || !map_region(physical_address,
            table_length, region)) {
        return 0;
    }
    if (!acpi_parse_sdt(region->bytes, table_length, signature, length)) {
        unmap_region(region);
        return 0;
    }
    return 1;
}

static int load_root_entries(const struct acpi_rsdp_info *rsdp_info,
    unsigned int *entry_count)
{
    struct acpi_mapped_region root;
    const char *signature;
    unsigned int physical_address;
    unsigned int table_length;
    unsigned int count;
    unsigned int index;
    unsigned int address;
    unsigned int high;
    int use_xsdt;

    if (rsdp_info == 0 || entry_count == 0) {
        return 0;
    }
    use_xsdt = rsdp_info->revision >= 2U &&
        rsdp_info->xsdt_address_high == 0U &&
        rsdp_info->xsdt_address_low >= 0x1000U;
    signature = use_xsdt ? "XSDT" : "RSDT";
    physical_address = use_xsdt ? rsdp_info->xsdt_address_low :
        rsdp_info->rsdt_address;
    if (!map_sdt(physical_address, signature, &root, &table_length)) {
        return 0;
    }
    if (use_xsdt) {
        if (!acpi_xsdt_entry_at(root.bytes, table_length, 0U, &address,
                &high, &count)) {
            unmap_region(&root);
            return 0;
        }
    } else if (!acpi_rsdt_entry_at(root.bytes, table_length, 0U, &address,
            &count)) {
        unmap_region(&root);
        return 0;
    }
    for (index = 0U; index < count; index++) {
        high = 0U;
        if ((use_xsdt && !acpi_xsdt_entry_at(root.bytes, table_length, index,
                &address, &high, &count)) ||
            (!use_xsdt && !acpi_rsdt_entry_at(root.bytes, table_length,
                index, &address, &count))) {
            unmap_region(&root);
            return 0;
        }
        rsdt_entries[index] = high == 0U ? address : 0U;
    }
    if (!unmap_region(&root)) {
        return 0;
    }
    *entry_count = count;
    return 1;
}

static int discover_hpet(unsigned int entry_count)
{
    unsigned int index;

    for (index = 0U; index < entry_count; index++) {
        struct acpi_mapped_region header;
        int is_hpet;

        if (rsdt_entries[index] == 0U ||
            !map_region(rsdt_entries[index], ACPI_SDT_HEADER_LENGTH,
                &header)) {
            continue;
        }
        is_hpet = signature_equal(header.bytes, "HPET");
        if (!unmap_region(&header)) {
            return 0;
        }
        if (is_hpet) {
            struct acpi_mapped_region table;
            struct acpi_hpet_table_info parsed;
            unsigned int length;

            if (!map_sdt(rsdt_entries[index], "HPET", &table, &length)) {
                return 0;
            }
            if (!acpi_parse_hpet_table(table.bytes, length, &parsed)) {
                unmap_region(&table);
                return 0;
            }
            hpet_info.event_timer_block_id = parsed.event_timer_block_id;
            hpet_info.physical_address = parsed.physical_address;
            hpet_info.minimum_tick = parsed.minimum_tick;
            hpet_info.sequence_number = parsed.sequence_number;
            hpet_info.page_protection = parsed.page_protection;
            if (!unmap_region(&table)) {
                return 0;
            }
            return 1;
        }
    }
    return 0;
}

int acpi_init(const struct boot_info *boot_info)
{
    const unsigned char *rsdp;
    struct acpi_rsdp_info parsed_rsdp;
    unsigned int maximum_length;
    unsigned int entry_count;

    initialized = 1;
    available = 0;
    hpet_available = 0;
    if (boot_info == 0 || boot_info->acpi_rsdp_address == 0U ||
        paging_current_directory() != paging_kernel_directory()) {
        return 0;
    }
    rsdp = (const unsigned char *)boot_info->acpi_rsdp_address;
    maximum_length = boot_info->acpi_rsdp_address < 0x000A0000U ?
        0x000A0000U - boot_info->acpi_rsdp_address :
        0x00100000U - boot_info->acpi_rsdp_address;
    if (!acpi_parse_rsdp(rsdp, maximum_length, &parsed_rsdp) ||
        !load_root_entries(&parsed_rsdp, &entry_count)) {
        return 0;
    }
    available = 1;
    hpet_available = discover_hpet(entry_count);
    return 1;
}

int acpi_available(void)
{
    return initialized && available;
}

int acpi_hpet_available(void)
{
    return initialized && available && hpet_available;
}

const struct acpi_hpet_info *acpi_hpet_get(void)
{
    return acpi_hpet_available() ? &hpet_info : 0;
}
