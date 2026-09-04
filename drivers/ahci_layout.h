#ifndef EFESOS_AHCI_LAYOUT_H
#define EFESOS_AHCI_LAYOUT_H

#include "pci.h"

#define AHCI_SECTOR_SIZE 512U
#define AHCI_MAX_TRANSFER_SECTORS 8U
#define AHCI_ATA_SIGNATURE 0x00000101U
#define AHCI_SCTL_DETECTION_MASK 0x0000000FU
#define AHCI_SCTL_DETECTION_COMRESET 0x00000001U

struct ahci_command_header {
    uint16_t flags;
    uint16_t prdt_length;
    uint32_t prd_byte_count;
    uint32_t command_table_base;
    uint32_t command_table_base_upper;
    uint32_t reserved[4];
} __attribute__((packed));

struct ahci_prd {
    uint32_t data_base;
    uint32_t data_base_upper;
    uint32_t reserved;
    uint32_t byte_count_and_flags;
} __attribute__((packed));

struct ahci_command_table {
    uint8_t command_fis[64];
    uint8_t atapi_command[16];
    uint8_t reserved[48];
    struct ahci_prd prdt[1];
} __attribute__((packed));

int ahci_port_is_usable_sata(uint32_t implemented_ports,
    unsigned int port, uint32_t sata_status, uint32_t signature);
uint32_t ahci_comreset_assert_control(uint32_t sata_control);
uint32_t ahci_comreset_release_control(uint32_t sata_control);
int ahci_link_is_active(uint32_t sata_status);
int ahci_link_is_established(uint32_t sata_status, uint32_t signature);
int ahci_identify_capacity(const uint16_t *identify, uint32_t *sector_count,
    int *lba48_supported);
int ahci_identify_same_device(const uint16_t *baseline,
    const uint16_t *candidate);
int ahci_build_identify_command(struct ahci_command_header *header,
    struct ahci_command_table *table, uint32_t table_physical,
    uint32_t data_physical);
int ahci_build_read_command(struct ahci_command_header *header,
    struct ahci_command_table *table, uint32_t table_physical,
    uint32_t data_physical, uint32_t lba, uint8_t count,
    int lba48_supported);

#endif
