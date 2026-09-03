#include "acpi_tables.h"

static unsigned int read_u16(const unsigned char *bytes)
{
    return (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8U);
}

static unsigned int read_u32(const unsigned char *bytes)
{
    return (unsigned int)bytes[0] |
        ((unsigned int)bytes[1] << 8U) |
        ((unsigned int)bytes[2] << 16U) |
        ((unsigned int)bytes[3] << 24U);
}

static int bytes_equal(const unsigned char *left, const char *right,
    unsigned int length)
{
    unsigned int index;

    for (index = 0U; index < length; index++) {
        if (left[index] != (unsigned char)right[index]) {
            return 0;
        }
    }
    return 1;
}

int acpi_checksum_valid(const unsigned char *bytes, unsigned int length)
{
    unsigned int index;
    unsigned char sum = 0U;

    if (bytes == 0 || length == 0U) {
        return 0;
    }
    for (index = 0U; index < length; index++) {
        sum = (unsigned char)(sum + bytes[index]);
    }
    return sum == 0U;
}

int acpi_parse_rsdp(const unsigned char *bytes, unsigned int available,
    struct acpi_rsdp_info *info)
{
    unsigned int revision;
    unsigned int length = ACPI_RSDP_V1_LENGTH;
    unsigned int rsdt_address;

    if (bytes == 0 || info == 0 || available < ACPI_RSDP_V1_LENGTH ||
        !bytes_equal(bytes, "RSD PTR ", 8U) ||
        !acpi_checksum_valid(bytes, ACPI_RSDP_V1_LENGTH)) {
        return 0;
    }
    revision = bytes[15];
    rsdt_address = read_u32(bytes + 16U);
    if (revision == 1U || rsdt_address < 0x1000U) {
        return 0;
    }
    if (revision >= 2U) {
        length = read_u32(bytes + 20U);
        if (length < ACPI_RSDP_V2_LENGTH || length > ACPI_MAX_RSDP_LENGTH ||
            length > available || !acpi_checksum_valid(bytes, length) ||
            bytes[33] != 0U || bytes[34] != 0U || bytes[35] != 0U) {
            return 0;
        }
    }
    info->rsdt_address = rsdt_address;
    info->revision = revision;
    info->length = length;
    return 1;
}

int acpi_parse_sdt(const unsigned char *bytes, unsigned int available,
    const char signature[4], unsigned int *length)
{
    unsigned int table_length;

    if (bytes == 0 || signature == 0 || length == 0 ||
        available < ACPI_SDT_HEADER_LENGTH ||
        !bytes_equal(bytes, signature, 4U)) {
        return 0;
    }
    table_length = read_u32(bytes + 4U);
    if (table_length < ACPI_SDT_HEADER_LENGTH ||
        table_length > ACPI_MAX_SDT_LENGTH || table_length > available ||
        !acpi_checksum_valid(bytes, table_length)) {
        return 0;
    }
    *length = table_length;
    return 1;
}

int acpi_rsdt_entry_at(const unsigned char *bytes, unsigned int available,
    unsigned int index, unsigned int *physical_address,
    unsigned int *entry_count)
{
    unsigned int length;
    unsigned int count;
    unsigned int address;

    if (physical_address == 0 || entry_count == 0 ||
        !acpi_parse_sdt(bytes, available, "RSDT", &length) ||
        ((length - ACPI_SDT_HEADER_LENGTH) & 3U) != 0U) {
        return 0;
    }
    count = (length - ACPI_SDT_HEADER_LENGTH) / 4U;
    if (count == 0U || count > ACPI_MAX_RSDT_ENTRIES || index >= count) {
        return 0;
    }
    address = read_u32(bytes + ACPI_SDT_HEADER_LENGTH + index * 4U);
    if (address < 0x1000U) {
        return 0;
    }
    *physical_address = address;
    *entry_count = count;
    return 1;
}

int acpi_parse_hpet_table(const unsigned char *bytes, unsigned int available,
    struct acpi_hpet_table_info *info)
{
    unsigned int length;
    unsigned int block_id;
    unsigned int address_low;
    unsigned int address_high;
    unsigned int vendor;
    unsigned char access_size;
    unsigned char page_protection;

    if (info == 0 || !acpi_parse_sdt(bytes, available, "HPET", &length) ||
        length < ACPI_HPET_TABLE_MIN_LENGTH || bytes[8] == 0U) {
        return 0;
    }
    block_id = read_u32(bytes + 36U);
    vendor = block_id >> 16U;
    access_size = bytes[43];
    address_low = read_u32(bytes + 44U);
    address_high = read_u32(bytes + 48U);
    page_protection = bytes[55];
    if ((block_id & (1U << 14U)) != 0U || vendor == 0U || vendor == 0xFFFFU ||
        bytes[40] != 0U || bytes[41] != 64U || bytes[42] != 0U ||
        (access_size != 0U && access_size != 4U) || address_high != 0U ||
        address_low < 0x1000U || (address_low & 0x3FFU) != 0U ||
        address_low > 0xFFFFFFFFU - 1023U || page_protection > 2U) {
        return 0;
    }
    info->event_timer_block_id = block_id;
    info->physical_address = address_low;
    info->minimum_tick = read_u16(bytes + 53U);
    info->sequence_number = bytes[52];
    info->page_protection = page_protection;
    return 1;
}
