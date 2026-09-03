#include "acpi_tables.h"

#include <stdio.h>

#define TEST_MADT_LENGTH (ACPI_MADT_HEADER_LENGTH + 8U + 12U + 10U + 10U)

static void write_u16(unsigned char *bytes, unsigned int value)
{
    bytes[0] = (unsigned char)value;
    bytes[1] = (unsigned char)(value >> 8U);
}

static void write_u32(unsigned char *bytes, unsigned int value)
{
    bytes[0] = (unsigned char)value;
    bytes[1] = (unsigned char)(value >> 8U);
    bytes[2] = (unsigned char)(value >> 16U);
    bytes[3] = (unsigned char)(value >> 24U);
}

static void set_checksum(unsigned char *bytes, unsigned int length)
{
    unsigned int index;
    unsigned char sum = 0U;

    bytes[9] = 0U;
    for (index = 0U; index < length; index++) {
        sum = (unsigned char)(sum + bytes[index]);
    }
    bytes[9] = (unsigned char)(0U - sum);
}

static void make_madt(unsigned char madt[TEST_MADT_LENGTH])
{
    unsigned int index;
    unsigned int offset;

    for (index = 0U; index < TEST_MADT_LENGTH; index++) {
        madt[index] = 0U;
    }
    madt[0] = 'A';
    madt[1] = 'P';
    madt[2] = 'I';
    madt[3] = 'C';
    write_u32(madt + 4U, TEST_MADT_LENGTH);
    madt[8] = 1U;
    write_u32(madt + 36U, 0xFEE00000U);
    write_u32(madt + 40U, 1U);

    offset = ACPI_MADT_HEADER_LENGTH;
    madt[offset] = 0U;
    madt[offset + 1U] = 8U;
    madt[offset + 2U] = 0U;
    madt[offset + 3U] = 2U;
    write_u32(madt + offset + 4U, 1U);
    offset += 8U;

    madt[offset] = 1U;
    madt[offset + 1U] = 12U;
    madt[offset + 2U] = 3U;
    write_u32(madt + offset + 4U, 0xFEC00000U);
    write_u32(madt + offset + 8U, 0U);
    offset += 12U;

    madt[offset] = 2U;
    madt[offset + 1U] = 10U;
    madt[offset + 2U] = 0U;
    madt[offset + 3U] = 0U;
    write_u32(madt + offset + 4U, 2U);
    write_u16(madt + offset + 8U, 0U);
    offset += 10U;

    madt[offset] = 2U;
    madt[offset + 1U] = 10U;
    madt[offset + 2U] = 0U;
    madt[offset + 3U] = 9U;
    write_u32(madt + offset + 4U, 9U);
    write_u16(madt + offset + 8U, 0x000FU);
    set_checksum(madt, TEST_MADT_LENGTH);
}

int main(void)
{
    unsigned char madt[TEST_MADT_LENGTH];
    unsigned char extended[TEST_MADT_LENGTH + 16U];
    struct acpi_madt_table_info info;
    unsigned int available;
    unsigned int index;

    make_madt(madt);
    if (!acpi_parse_madt_table(madt, sizeof(madt), &info) ||
        info.local_apic_address != 0xFEE00000U || info.flags != 1U ||
        info.enabled_local_apics != 1U || info.enabled_x2apics != 0U ||
        info.io_apic_count != 1U || info.io_apics[0].id != 3U ||
        info.io_apics[0].physical_address != 0xFEC00000U ||
        info.io_apics[0].global_interrupt_base != 0U ||
        info.isa_overrides[0].present != 1U ||
        info.isa_overrides[0].global_interrupt != 2U ||
        info.isa_overrides[9].flags != 0x000FU ||
        info.isa_overrides[1].present != 0U ||
        info.isa_overrides[1].global_interrupt != 1U) {
        return 1;
    }
    for (available = 0U; available < sizeof(madt); available++) {
        if (acpi_parse_madt_table(madt, available, &info)) {
            return 1;
        }
    }

    madt[20] ^= 1U;
    if (acpi_parse_madt_table(madt, sizeof(madt), &info)) {
        return 1;
    }
    make_madt(madt);
    write_u32(madt + 40U, 3U);
    set_checksum(madt, sizeof(madt));
    if (acpi_parse_madt_table(madt, sizeof(madt), &info)) {
        return 1;
    }
    make_madt(madt);
    write_u32(madt + ACPI_MADT_HEADER_LENGTH + 4U, 3U);
    set_checksum(madt, sizeof(madt));
    if (acpi_parse_madt_table(madt, sizeof(madt), &info)) {
        return 1;
    }
    make_madt(madt);
    madt[ACPI_MADT_HEADER_LENGTH + 8U + 1U] = 0U;
    set_checksum(madt, sizeof(madt));
    if (acpi_parse_madt_table(madt, sizeof(madt), &info)) {
        return 1;
    }
    make_madt(madt);
    write_u32(madt + ACPI_MADT_HEADER_LENGTH + 8U + 4U, 0xFEC00004U);
    set_checksum(madt, sizeof(madt));
    if (acpi_parse_madt_table(madt, sizeof(madt), &info)) {
        return 1;
    }
    make_madt(madt);
    madt[ACPI_MADT_HEADER_LENGTH + 8U + 12U + 10U + 3U] = 0U;
    set_checksum(madt, sizeof(madt));
    if (acpi_parse_madt_table(madt, sizeof(madt), &info)) {
        return 1;
    }
    make_madt(madt);
    write_u16(madt + ACPI_MADT_HEADER_LENGTH + 8U + 12U + 8U, 2U);
    set_checksum(madt, sizeof(madt));
    if (acpi_parse_madt_table(madt, sizeof(madt), &info)) {
        return 1;
    }

    make_madt(madt);
    for (index = 0U; index < sizeof(madt); index++) {
        extended[index] = madt[index];
    }
    for (; index < sizeof(extended); index++) {
        extended[index] = 0U;
    }
    extended[TEST_MADT_LENGTH] = 5U;
    extended[TEST_MADT_LENGTH + 1U] = 12U;
    write_u32(extended + 36U, 0U);
    write_u32(extended + TEST_MADT_LENGTH + 4U, 0xFEE01000U);
    write_u32(extended + 4U, TEST_MADT_LENGTH + 12U);
    set_checksum(extended, TEST_MADT_LENGTH + 12U);
    if (!acpi_parse_madt_table(extended, TEST_MADT_LENGTH + 12U, &info) ||
        info.local_apic_address != 0xFEE01000U) {
        return 1;
    }
    write_u32(extended + TEST_MADT_LENGTH + 8U, 1U);
    set_checksum(extended, TEST_MADT_LENGTH + 12U);
    if (acpi_parse_madt_table(extended, TEST_MADT_LENGTH + 12U, &info)) {
        return 1;
    }

    make_madt(madt);
    write_u32(madt + ACPI_MADT_HEADER_LENGTH + 4U, 0U);
    for (index = 0U; index < sizeof(madt); index++) {
        extended[index] = madt[index];
    }
    for (; index < sizeof(extended); index++) {
        extended[index] = 0U;
    }
    extended[TEST_MADT_LENGTH] = 9U;
    extended[TEST_MADT_LENGTH + 1U] = 16U;
    write_u32(extended + TEST_MADT_LENGTH + 4U, 0x00000100U);
    write_u32(extended + TEST_MADT_LENGTH + 8U, 1U);
    write_u32(extended + TEST_MADT_LENGTH + 12U, 7U);
    write_u32(extended + 4U, sizeof(extended));
    set_checksum(extended, sizeof(extended));
    if (!acpi_parse_madt_table(extended, sizeof(extended), &info) ||
        info.enabled_local_apics != 0U || info.enabled_x2apics != 1U) {
        return 1;
    }

    make_madt(madt);
    for (index = 0U; index < sizeof(madt); index++) {
        extended[index] = madt[index];
    }
    extended[TEST_MADT_LENGTH] = 0x7FU;
    extended[TEST_MADT_LENGTH + 1U] = 2U;
    write_u32(extended + 4U, TEST_MADT_LENGTH + 2U);
    set_checksum(extended, TEST_MADT_LENGTH + 2U);
    if (!acpi_parse_madt_table(extended, TEST_MADT_LENGTH + 2U, &info)) {
        return 1;
    }

    puts("ACPI MADT host self-test passed.");
    return 0;
}
