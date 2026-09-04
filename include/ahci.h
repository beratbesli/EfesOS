#ifndef EFESOS_AHCI_H
#define EFESOS_AHCI_H

#include "block_device.h"

enum ahci_error {
    AHCI_ERROR_NONE = 0,
    AHCI_ERROR_NO_CONTROLLER,
    AHCI_ERROR_PCI_COMMAND,
    AHCI_ERROR_MMIO_MAPPING,
    AHCI_ERROR_CAPABILITIES,
    AHCI_ERROR_BIOS_HANDOFF,
    AHCI_ERROR_NO_SATA_DEVICE,
    AHCI_ERROR_MEMORY,
    AHCI_ERROR_PORT_STOP,
    AHCI_ERROR_PORT_START,
    AHCI_ERROR_COMMAND_TIMEOUT,
    AHCI_ERROR_COMMAND_STATUS,
    AHCI_ERROR_TRANSFER_COUNT,
    AHCI_ERROR_IDENTIFY,
    AHCI_ERROR_BLOCK_DEVICE
};

void ahci_init(void);
int ahci_present(void);
unsigned int ahci_sector_count(void);
unsigned int ahci_port_number(void);
unsigned int ahci_version(void);
unsigned int ahci_last_error(void);
unsigned int ahci_read_count(void);
const struct block_device *ahci_block_device(void);

#endif
