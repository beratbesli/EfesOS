#include "ahci_layout.h"

static int test_port_selection(void)
{
    return ahci_port_is_usable_sata(1U, 0U, 0x00000103U,
            AHCI_ATA_SIGNATURE) &&
        !ahci_port_is_usable_sata(0U, 0U, 0x00000103U,
            AHCI_ATA_SIGNATURE) &&
        !ahci_port_is_usable_sata(1U, 32U, 0x00000103U,
            AHCI_ATA_SIGNATURE) &&
        !ahci_port_is_usable_sata(1U, 0U, 0x00000101U,
            AHCI_ATA_SIGNATURE) &&
        !ahci_port_is_usable_sata(1U, 0U, 0x00000203U,
            AHCI_ATA_SIGNATURE) &&
        !ahci_port_is_usable_sata(1U, 0U, 0x00000103U, 0xEB140101U);
}

static int test_identify(void)
{
    uint16_t identify[256] = {0};
    uint32_t sectors = 0U;
    int lba48 = 0;

    identify[49] = 0x0200U;
    identify[60] = 0x1234U;
    identify[61] = 0x0001U;
    if (!ahci_identify_capacity(identify, &sectors, &lba48) ||
        sectors != 0x00011234U || lba48) {
        return 0;
    }
    identify[83] = 0x0400U;
    identify[100] = 0x5678U;
    identify[101] = 0x1234U;
    if (!ahci_identify_capacity(identify, &sectors, &lba48) ||
        sectors != 0x12345678U || !lba48) {
        return 0;
    }
    identify[102] = 1U;
    if (ahci_identify_capacity(identify, &sectors, &lba48)) {
        return 0;
    }
    identify[102] = 0U;
    identify[49] = 0U;
    return !ahci_identify_capacity(identify, &sectors, &lba48) &&
        !ahci_identify_capacity(0, &sectors, &lba48) &&
        !ahci_identify_capacity(identify, 0, &lba48) &&
        !ahci_identify_capacity(identify, &sectors, 0);
}

static int test_commands(void)
{
    struct ahci_command_header header;
    struct ahci_command_table table;

    if (!ahci_build_identify_command(&header, &table, 0x00101000U,
            0x00102000U) || header.flags != 5U ||
        header.prdt_length != 1U || header.prd_byte_count != 0U ||
        header.command_table_base != 0x00101000U ||
        table.command_fis[0] != 0x27U || table.command_fis[1] != 0x80U ||
        table.command_fis[2] != 0xECU ||
        table.prdt[0].data_base != 0x00102000U ||
        table.prdt[0].byte_count_and_flags != 511U) {
        return 0;
    }
    if (!ahci_build_read_command(&header, &table, 0x00101000U,
            0x00102000U, 0x12345678U, 8U, 1) ||
        table.command_fis[2] != 0x25U || table.command_fis[4] != 0x78U ||
        table.command_fis[5] != 0x56U || table.command_fis[6] != 0x34U ||
        table.command_fis[7] != 0x40U || table.command_fis[8] != 0x12U ||
        table.command_fis[12] != 8U || table.command_fis[13] != 0U ||
        table.prdt[0].byte_count_and_flags != 4095U) {
        return 0;
    }
    if (!ahci_build_read_command(&header, &table, 0x00101000U,
            0x00102000U, 0x01234567U, 1U, 0) ||
        table.command_fis[2] != 0xC8U || table.command_fis[7] != 0xE1U) {
        return 0;
    }
    return !ahci_build_identify_command(0, &table, 0x00101000U,
            0x00102000U) &&
        !ahci_build_identify_command(&header, &table, 0x00101001U,
            0x00102000U) &&
        !ahci_build_read_command(&header, &table, 0x00101000U,
            0x00102000U, 0U, 0U, 1) &&
        !ahci_build_read_command(&header, &table, 0x00101000U,
            0x00102000U, 0U, 9U, 1) &&
        !ahci_build_read_command(&header, &table, 0x00101000U,
            0x00102000U, 0x0FFFFFFFU, 2U, 0);
}

int main(void)
{
    return test_port_selection() && test_identify() && test_commands() ? 0 : 1;
}
