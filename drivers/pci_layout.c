#include "pci.h"

int pci_ahci_mmio_base(const struct pci_device *device, uint32_t *base)
{
    const struct pci_bar *abar;

    if (device == 0 || base == 0 || (device->header_type & 0x7FU) != 0U ||
        device->class_code != 0x01U || device->subclass != 0x06U ||
        device->prog_if != 0x01U) {
        return 0;
    }

    abar = &device->bars[5];
    if (abar->type != PCI_BAR_MEMORY32 || abar->base_low == 0U ||
        abar->base_high != 0U || abar->flags != 0U ||
        (abar->base_low & 0x0FU) != 0U) {
        return 0;
    }

    *base = abar->base_low;
    return 1;
}
