#include "pci.h"

#define PCI_CONFIG_ADDRESS 0x0CF8U
#define PCI_CONFIG_DATA 0x0CFCU

static struct pci_device devices[PCI_MAX_DEVICES];
static unsigned int device_count;

static uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset)
{
    uint32_t address = 0x80000000U |
        ((uint32_t)bus << 16U) |
        ((uint32_t)slot << 11U) |
        ((uint32_t)function << 8U) |
        ((uint32_t)offset & 0xFCU);

    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset)
{
    uint32_t value = pci_config_read(bus, slot, function, offset);

    return (uint16_t)((value >> ((offset & 2U) * 8U)) & 0xFFFFU);
}

static int pci_is_present(uint8_t bus, uint8_t slot, uint8_t function)
{
    return pci_config_read_word(bus, slot, function, 0) != 0xFFFFU;
}

static void pci_record(uint8_t bus, uint8_t slot, uint8_t function)
{
    uint32_t class_register;
    struct pci_device *device;

    if (device_count == PCI_MAX_DEVICES || !pci_is_present(bus, slot, function)) {
        return;
    }

    device = &devices[device_count++];
    class_register = pci_config_read(bus, slot, function, 8);
    device->bus = bus;
    device->slot = slot;
    device->function = function;
    device->vendor_id = pci_config_read_word(bus, slot, function, 0);
    device->device_id = pci_config_read_word(bus, slot, function, 2);
    device->revision = (uint8_t)(class_register & 0xFFU);
    device->prog_if = (uint8_t)((class_register >> 8U) & 0xFFU);
    device->subclass = (uint8_t)((class_register >> 16U) & 0xFFU);
    device->class_code = (uint8_t)((class_register >> 24U) & 0xFFU);
    device->interrupt_line = (uint8_t)(pci_config_read(bus, slot, function, 0x3C) & 0xFFU);
}

void pci_init(void)
{
    unsigned int bus;
    unsigned int slot;

    device_count = 0;
    for (bus = 0; bus < 256U && device_count < PCI_MAX_DEVICES; bus++) {
        for (slot = 0; slot < 32U && device_count < PCI_MAX_DEVICES; slot++) {
            uint8_t function;
            uint8_t header_type;

            if (!pci_is_present((uint8_t)bus, (uint8_t)slot, 0)) {
                continue;
            }
            pci_record((uint8_t)bus, (uint8_t)slot, 0);
            header_type = (uint8_t)((pci_config_read((uint8_t)bus, (uint8_t)slot, 0, 0x0C) >> 16U) & 0xFFU);
            if ((header_type & 0x80U) == 0U) {
                continue;
            }
            for (function = 1; function < 8U && device_count < PCI_MAX_DEVICES; function++) {
                pci_record((uint8_t)bus, (uint8_t)slot, function);
            }
        }
    }
}

unsigned int pci_device_count(void)
{
    return device_count;
}

const struct pci_device *pci_device_at(unsigned int index)
{
    if (index >= device_count) {
        return 0;
    }
    return &devices[index];
}
