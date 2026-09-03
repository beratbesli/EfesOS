#include <stdio.h>
#include <string.h>

#include "acpi_tables.h"

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

static void set_checksum(unsigned char *bytes, unsigned int length,
    unsigned int checksum_offset)
{
    unsigned int index;
    unsigned char sum = 0U;

    bytes[checksum_offset] = 0U;
    for (index = 0U; index < length; index++) {
        sum = (unsigned char)(sum + bytes[index]);
    }
    bytes[checksum_offset] = (unsigned char)(0U - sum);
}

static void make_rsdp(unsigned char rsdp[ACPI_RSDP_V2_LENGTH])
{
    memset(rsdp, 0, ACPI_RSDP_V2_LENGTH);
    memcpy(rsdp, "RSD PTR ", 8U);
    memcpy(rsdp + 9U, "EFESOS", 6U);
    rsdp[15] = 2U;
    write_u32(rsdp + 16U, 0x00123000U);
    write_u32(rsdp + 20U, ACPI_RSDP_V2_LENGTH);
    set_checksum(rsdp, ACPI_RSDP_V1_LENGTH, 8U);
    set_checksum(rsdp, ACPI_RSDP_V2_LENGTH, 32U);
}

static void make_sdt(unsigned char *table, unsigned int length,
    const char signature[4])
{
    memset(table, 0, length);
    memcpy(table, signature, 4U);
    write_u32(table + 4U, length);
    table[8] = 1U;
    memcpy(table + 10U, "EFESOS", 6U);
    set_checksum(table, length, 9U);
}

int main(void)
{
    unsigned char rsdp[ACPI_RSDP_V2_LENGTH];
    unsigned char rsdt[ACPI_SDT_HEADER_LENGTH + 8U];
    unsigned char hpet[ACPI_HPET_TABLE_MIN_LENGTH];
    struct acpi_rsdp_info rsdp_info;
    struct acpi_hpet_table_info hpet_info;
    unsigned int length;
    unsigned int address;
    unsigned int count;
    unsigned int index;

    make_rsdp(rsdp);
    if (!acpi_parse_rsdp(rsdp, sizeof(rsdp), &rsdp_info) ||
        rsdp_info.rsdt_address != 0x00123000U || rsdp_info.revision != 2U ||
        rsdp_info.length != ACPI_RSDP_V2_LENGTH) {
        return 1;
    }
    for (index = 0U; index < sizeof(rsdp); index++) {
        unsigned char saved = rsdp[index];
        rsdp[index] ^= 1U;
        if (acpi_parse_rsdp(rsdp, sizeof(rsdp), &rsdp_info)) {
            return 1;
        }
        rsdp[index] = saved;
    }

    make_sdt(rsdt, sizeof(rsdt), "RSDT");
    write_u32(rsdt + ACPI_SDT_HEADER_LENGTH, 0x00200000U);
    write_u32(rsdt + ACPI_SDT_HEADER_LENGTH + 4U, 0x00201000U);
    set_checksum(rsdt, sizeof(rsdt), 9U);
    if (!acpi_rsdt_entry_at(rsdt, sizeof(rsdt), 1U, &address, &count) ||
        address != 0x00201000U || count != 2U ||
        acpi_rsdt_entry_at(rsdt, sizeof(rsdt), 2U, &address, &count)) {
        return 1;
    }

    make_sdt(hpet, sizeof(hpet), "HPET");
    write_u32(hpet + 36U, 0x80860101U);
    hpet[40] = 0U;
    hpet[41] = 64U;
    hpet[42] = 0U;
    hpet[43] = 0U;
    write_u32(hpet + 44U, 0xFED00000U);
    write_u32(hpet + 48U, 0U);
    hpet[52] = 0U;
    write_u16(hpet + 53U, 128U);
    hpet[55] = 0U;
    set_checksum(hpet, sizeof(hpet), 9U);
    if (!acpi_parse_hpet_table(hpet, sizeof(hpet), &hpet_info) ||
        hpet_info.physical_address != 0xFED00000U ||
        hpet_info.event_timer_block_id != 0x80860101U ||
        hpet_info.minimum_tick != 128U || hpet_info.sequence_number != 0U) {
        return 1;
    }
    hpet[48] = 1U;
    set_checksum(hpet, sizeof(hpet), 9U);
    if (acpi_parse_hpet_table(hpet, sizeof(hpet), &hpet_info)) {
        return 1;
    }
    hpet[48] = 0U;
    hpet[44] = 1U;
    set_checksum(hpet, sizeof(hpet), 9U);
    if (acpi_parse_hpet_table(hpet, sizeof(hpet), &hpet_info) ||
        acpi_parse_sdt(hpet, ACPI_SDT_HEADER_LENGTH - 1U, "HPET", &length) ||
        acpi_checksum_valid(0, 1U)) {
        return 1;
    }
    puts("ACPI table host self-test passed.");
    return 0;
}
