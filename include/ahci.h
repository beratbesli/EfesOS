#ifndef EFESOS_AHCI_H
#define EFESOS_AHCI_H

#include "block_device.h"

#define AHCI_MSI_VECTOR 0x33U

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
    AHCI_ERROR_RESET_DELAY,
    AHCI_ERROR_RESET_LINK,
    AHCI_ERROR_RESET_ENGINE,
    AHCI_ERROR_RESET_IDENTITY,
    AHCI_ERROR_HBA_RESET_TIMEOUT,
    AHCI_ERROR_HBA_RESET_STATE,
    AHCI_ERROR_HBA_RESET_DELAY,
    AHCI_ERROR_HBA_RESET_LINK,
    AHCI_ERROR_HBA_RESET_IDENTITY,
    AHCI_ERROR_BLOCK_DEVICE,
    AHCI_ERROR_INTERRUPT
};

void ahci_init(void);
int ahci_enable_irq_mode(unsigned int apic_id);
void ahci_irq_handler(void);
int ahci_irq_mode_enabled(void);
unsigned int ahci_irq_count(void);
unsigned int ahci_irq_fallback_count(void);
int ahci_present(void);
unsigned int ahci_sector_count(void);
unsigned int ahci_controller_index(void);
unsigned int ahci_controller_probe_count(void);
unsigned int ahci_controller_failover_count(void);
unsigned int ahci_port_number(void);
unsigned int ahci_version(void);
unsigned int ahci_last_error(void);
unsigned int ahci_read_count(void);
unsigned int ahci_recovery_count(void);
unsigned int ahci_recovery_attempt_count(void);
unsigned int ahci_port_reset_attempt_count(void);
unsigned int ahci_hba_reset_attempt_count(void);
unsigned int ahci_hba_reset_count(void);
int ahci_fail_closed(void);
const struct block_device *ahci_block_device(void);

#endif
