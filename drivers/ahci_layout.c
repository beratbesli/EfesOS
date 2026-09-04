#include "ahci_layout.h"

#define AHCI_FIS_TYPE_REGISTER_H2D 0x27U
#define AHCI_FIS_COMMAND 0x80U
#define AHCI_COMMAND_FIS_DWORDS 5U
#define AHCI_ATA_CMD_IDENTIFY 0xECU
#define AHCI_ATA_CMD_READ_DMA 0xC8U
#define AHCI_ATA_CMD_READ_DMA_EXT 0x25U
#define AHCI_LBA28_LIMIT 0x10000000U

_Static_assert(sizeof(struct ahci_command_header) == 32U,
    "AHCI command header must be 32 bytes");
_Static_assert(sizeof(struct ahci_prd) == 16U,
    "AHCI PRD must be 16 bytes");
_Static_assert(sizeof(struct ahci_command_table) == 144U,
    "AHCI command table with one PRD must be 144 bytes");

static void clear_bytes(void *memory, unsigned int length)
{
    uint8_t *bytes = (uint8_t *)memory;
    unsigned int index;

    for (index = 0U; index < length; index++) {
        bytes[index] = 0U;
    }
}

static int words_equal(const uint16_t *left, const uint16_t *right,
    unsigned int first, unsigned int count)
{
    unsigned int index;

    for (index = 0U; index < count; index++) {
        if (left[first + index] != right[first + index]) {
            return 0;
        }
    }
    return 1;
}

static int prepare_data_command(struct ahci_command_header *header,
    struct ahci_command_table *table, uint32_t table_physical,
    uint32_t data_physical, unsigned int byte_count)
{
    if (header == 0 || table == 0 || byte_count == 0U ||
        byte_count > AHCI_SECTOR_SIZE * AHCI_MAX_TRANSFER_SECTORS ||
        (table_physical & 0x7FU) != 0U ||
        (data_physical & 1U) != 0U) {
        return 0;
    }

    clear_bytes(header, sizeof(*header));
    clear_bytes(table, sizeof(*table));
    header->flags = AHCI_COMMAND_FIS_DWORDS;
    header->prdt_length = 1U;
    header->command_table_base = table_physical;
    table->prdt[0].data_base = data_physical;
    table->prdt[0].byte_count_and_flags = byte_count - 1U;
    table->command_fis[0] = AHCI_FIS_TYPE_REGISTER_H2D;
    table->command_fis[1] = AHCI_FIS_COMMAND;
    return 1;
}

int ahci_port_is_usable_sata(uint32_t implemented_ports,
    unsigned int port, uint32_t sata_status, uint32_t signature)
{
    return port < 32U && (implemented_ports & (1U << port)) != 0U &&
        ahci_link_is_established(sata_status, signature);
}

uint32_t ahci_comreset_assert_control(uint32_t sata_control)
{
    return (sata_control & ~AHCI_SCTL_DETECTION_MASK) |
        AHCI_SCTL_DETECTION_COMRESET;
}

uint32_t ahci_comreset_release_control(uint32_t sata_control)
{
    return sata_control & ~AHCI_SCTL_DETECTION_MASK;
}

int ahci_link_is_active(uint32_t sata_status)
{
    unsigned int detection = sata_status & 0x0FU;
    unsigned int power = (sata_status >> 8U) & 0x0FU;

    return detection == 3U && power == 1U;
}

int ahci_link_is_established(uint32_t sata_status, uint32_t signature)
{
    return ahci_link_is_active(sata_status) && signature == AHCI_ATA_SIGNATURE;
}

int ahci_identify_capacity(const uint16_t *identify, uint32_t *sector_count,
    int *lba48_supported)
{
    uint32_t count;
    int supports_lba48;

    if (identify == 0 || sector_count == 0 || lba48_supported == 0 ||
        (identify[49] & 0x0200U) == 0U) {
        return 0;
    }

    supports_lba48 = (identify[83] & 0x0400U) != 0U;
    if (supports_lba48) {
        if (identify[103] != 0U || identify[102] != 0U) {
            return 0;
        }
        count = ((uint32_t)identify[101] << 16U) | identify[100];
    } else {
        count = ((uint32_t)identify[61] << 16U) | identify[60];
        if (count > AHCI_LBA28_LIMIT) {
            return 0;
        }
    }
    if (count == 0U) {
        return 0;
    }

    *sector_count = count;
    *lba48_supported = supports_lba48;
    return 1;
}

int ahci_identify_same_device(const uint16_t *baseline,
    const uint16_t *candidate)
{
    uint32_t baseline_sectors;
    uint32_t candidate_sectors;
    int baseline_lba48;
    int candidate_lba48;

    if (baseline == 0 || candidate == 0 ||
        !ahci_identify_capacity(baseline, &baseline_sectors,
            &baseline_lba48) ||
        !ahci_identify_capacity(candidate, &candidate_sectors,
            &candidate_lba48) ||
        baseline_sectors != candidate_sectors ||
        baseline_lba48 != candidate_lba48 ||
        baseline[0] != candidate[0] || baseline[49] != candidate[49] ||
        !words_equal(baseline, candidate, 10U, 10U) ||
        !words_equal(baseline, candidate, 23U, 24U)) {
        return 0;
    }
    return 1;
}

int ahci_build_identify_command(struct ahci_command_header *header,
    struct ahci_command_table *table, uint32_t table_physical,
    uint32_t data_physical)
{
    if (!prepare_data_command(header, table, table_physical, data_physical,
            AHCI_SECTOR_SIZE)) {
        return 0;
    }
    table->command_fis[2] = AHCI_ATA_CMD_IDENTIFY;
    return 1;
}

int ahci_build_read_command(struct ahci_command_header *header,
    struct ahci_command_table *table, uint32_t table_physical,
    uint32_t data_physical, uint32_t lba, uint8_t count,
    int lba48_supported)
{
    uint8_t *fis;

    if (count == 0U || count > AHCI_MAX_TRANSFER_SECTORS ||
        (!lba48_supported &&
            (lba >= AHCI_LBA28_LIMIT ||
             (uint32_t)count > AHCI_LBA28_LIMIT - lba)) ||
        !prepare_data_command(header, table, table_physical, data_physical,
            (unsigned int)count * AHCI_SECTOR_SIZE)) {
        return 0;
    }

    fis = table->command_fis;
    fis[2] = lba48_supported ? AHCI_ATA_CMD_READ_DMA_EXT : AHCI_ATA_CMD_READ_DMA;
    fis[4] = (uint8_t)lba;
    fis[5] = (uint8_t)(lba >> 8U);
    fis[6] = (uint8_t)(lba >> 16U);
    fis[7] = lba48_supported ? 0x40U :
        (uint8_t)(0xE0U | ((lba >> 24U) & 0x0FU));
    if (lba48_supported) {
        fis[8] = (uint8_t)(lba >> 24U);
        fis[12] = count;
        fis[13] = 0U;
    } else {
        fis[12] = count;
    }
    return 1;
}
