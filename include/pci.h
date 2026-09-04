#ifndef EFESOS_PCI_H
#define EFESOS_PCI_H

#include "../cpu/io.h"

#define PCI_MAX_DEVICES 32U
#define PCI_MAX_BARS 6U

enum pci_bar_type {
    PCI_BAR_UNUSED,
    PCI_BAR_IO,
    PCI_BAR_MEMORY32,
    PCI_BAR_MEMORY64
};

struct pci_bar {
    uint32_t base_low;
    uint32_t base_high;
    uint32_t flags;
    uint8_t type;
};

struct pci_device {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision;
    uint8_t interrupt_line;
    uint8_t header_type;
    struct pci_bar bars[PCI_MAX_BARS];
};

void pci_init(void);
int pci_self_test(void);
int pci_enable_ide_bus_master(const struct pci_device *device);
int pci_disable_ide_bus_master(const struct pci_device *device);
int pci_ahci_mmio_base(const struct pci_device *device, uint32_t *base);
unsigned int pci_ahci_controller_count(void);
unsigned int pci_ahci_usable_count(void);
const struct pci_device *pci_ahci_device_at(unsigned int index);
int pci_prepare_ahci_controller(const struct pci_device *device,
    uint16_t *original_command);
int pci_enable_ahci_bus_master(const struct pci_device *device);
int pci_enable_ahci_msi(const struct pci_device *device,
    unsigned int apic_id, unsigned int vector);
int pci_disable_ahci_msi(const struct pci_device *device);
int pci_quiesce_ahci_controller(const struct pci_device *device,
    uint16_t original_command);
unsigned int pci_device_count(void);
const struct pci_device *pci_device_at(unsigned int index);

#endif
