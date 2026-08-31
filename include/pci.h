#ifndef EFESOS_PCI_H
#define EFESOS_PCI_H

#include "../cpu/io.h"

#define PCI_MAX_DEVICES 32U

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
};

void pci_init(void);
unsigned int pci_device_count(void);
const struct pci_device *pci_device_at(unsigned int index);

#endif
