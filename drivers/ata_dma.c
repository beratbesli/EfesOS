#include "ata_dma.h"

#define ATA_DMA_BOUNDARY 0x10000U
#define ATA_DMA_CAPABILITY_BIT 0x0100U
#define ATA_DMA_MULTIWORD_SUPPORTED_MASK 0x0007U
#define ATA_DMA_MULTIWORD_BASE 0x20U
#define PCI_CLASS_MASS_STORAGE 0x01U
#define PCI_SUBCLASS_IDE 0x01U
#define PCI_IDE_PRIMARY_NATIVE 0x01U
#define PCI_IDE_BUS_MASTER_CAPABLE 0x80U
#define PCI_IO_BASE_MINIMUM 0x0100U
#define PCI_IO_BASE_MAXIMUM 0xFFF0U

_Static_assert(sizeof(struct ata_dma_prd) == 8U,
    "ATA DMA PRD must be exactly eight bytes");

int ata_dma_prepare_prd(struct ata_dma_prd *prd,
    unsigned int physical_base, unsigned int byte_count)
{
    unsigned int boundary_remaining;

    if (prd == 0 || (physical_base & 3U) != 0U || byte_count == 0U ||
        byte_count > ATA_DMA_BOUNDARY) {
        return 0;
    }
    boundary_remaining = ATA_DMA_BOUNDARY -
        (physical_base & (ATA_DMA_BOUNDARY - 1U));
    if (byte_count > boundary_remaining) {
        return 0;
    }

    prd->physical_base = physical_base;
    prd->byte_count = byte_count == ATA_DMA_BOUNDARY ?
        0U : (unsigned short)byte_count;
    prd->flags = ATA_DMA_PRD_END;
    return 1;
}

int ata_dma_controller_base(const struct pci_device *device,
    unsigned short *io_base)
{
    const struct pci_bar *bar;

    if (device == 0 || io_base == 0 ||
        (device->header_type & 0x7FU) != 0U ||
        device->class_code != PCI_CLASS_MASS_STORAGE ||
        device->subclass != PCI_SUBCLASS_IDE ||
        (device->prog_if & PCI_IDE_BUS_MASTER_CAPABLE) == 0U ||
        (device->prog_if & PCI_IDE_PRIMARY_NATIVE) != 0U) {
        return 0;
    }
    bar = &device->bars[4];
    if (bar->type != PCI_BAR_IO || (bar->flags & 1U) == 0U ||
        bar->base_low < PCI_IO_BASE_MINIMUM ||
        bar->base_low > PCI_IO_BASE_MAXIMUM ||
        (bar->base_low & 0x0FU) != 0U) {
        return 0;
    }
    *io_base = (unsigned short)bar->base_low;
    return 1;
}

int ata_dma_select_multiword_mode(unsigned short capabilities,
    unsigned short multiword_dma, unsigned char *transfer_mode)
{
    unsigned short supported;
    unsigned int mode;

    if (transfer_mode == 0 ||
        (capabilities & ATA_DMA_CAPABILITY_BIT) == 0U) {
        return 0;
    }
    supported = multiword_dma & ATA_DMA_MULTIWORD_SUPPORTED_MASK;
    for (mode = 3U; mode != 0U; mode--) {
        unsigned int candidate = mode - 1U;

        if ((supported & (1U << candidate)) != 0U) {
            *transfer_mode = (unsigned char)(ATA_DMA_MULTIWORD_BASE + candidate);
            return 1;
        }
    }
    return 0;
}

int ata_dma_status_is_complete(unsigned char status)
{
    return (status & (ATA_DMA_STATUS_ACTIVE | ATA_DMA_STATUS_ERROR |
        ATA_DMA_STATUS_INTERRUPT)) == ATA_DMA_STATUS_INTERRUPT;
}
