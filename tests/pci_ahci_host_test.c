#include "pci.h"

static struct pci_device valid_device(void)
{
    struct pci_device device = {0};

    device.class_code = 0x01U;
    device.subclass = 0x06U;
    device.prog_if = 0x01U;
    device.header_type = 0x80U;
    device.bars[5].type = PCI_BAR_MEMORY32;
    device.bars[5].base_low = 0xFEBF1000U;
    return device;
}

int main(void)
{
    struct pci_device device = valid_device();
    uint32_t base = 0U;

    if (!pci_ahci_mmio_base(&device, &base) || base != 0xFEBF1000U ||
        pci_ahci_mmio_base(0, &base) ||
        pci_ahci_mmio_base(&device, 0)) {
        return 1;
    }

    device.class_code = 0x02U;
    if (pci_ahci_mmio_base(&device, &base)) {
        return 2;
    }
    device = valid_device();
    device.subclass = 0x01U;
    if (pci_ahci_mmio_base(&device, &base)) {
        return 3;
    }
    device = valid_device();
    device.prog_if = 0x00U;
    if (pci_ahci_mmio_base(&device, &base)) {
        return 4;
    }
    device = valid_device();
    device.header_type = 0x01U;
    if (pci_ahci_mmio_base(&device, &base)) {
        return 5;
    }
    device = valid_device();
    device.bars[5].type = PCI_BAR_IO;
    if (pci_ahci_mmio_base(&device, &base)) {
        return 6;
    }
    device = valid_device();
    device.bars[5].type = PCI_BAR_MEMORY64;
    if (pci_ahci_mmio_base(&device, &base)) {
        return 7;
    }
    device = valid_device();
    device.bars[5].base_low = 0U;
    if (pci_ahci_mmio_base(&device, &base)) {
        return 8;
    }
    device = valid_device();
    device.bars[5].base_high = 1U;
    if (pci_ahci_mmio_base(&device, &base)) {
        return 9;
    }
    device = valid_device();
    device.bars[5].flags = 0x08U;
    if (pci_ahci_mmio_base(&device, &base)) {
        return 10;
    }
    device = valid_device();
    device.bars[5].base_low |= 4U;
    return pci_ahci_mmio_base(&device, &base) ? 11 : 0;
}
