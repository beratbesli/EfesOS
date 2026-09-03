#ifndef EFESOS_ATA_DMA_H
#define EFESOS_ATA_DMA_H

#include "pci.h"

#define ATA_DMA_PRD_END 0x8000U
#define ATA_DMA_STATUS_ACTIVE 0x01U
#define ATA_DMA_STATUS_ERROR 0x02U
#define ATA_DMA_STATUS_INTERRUPT 0x04U

struct ata_dma_prd {
    unsigned int physical_base;
    unsigned short byte_count;
    unsigned short flags;
};

int ata_dma_prepare_prd(struct ata_dma_prd *prd,
    unsigned int physical_base, unsigned int byte_count);
int ata_dma_controller_base(const struct pci_device *device,
    unsigned short *io_base);
int ata_dma_select_multiword_mode(unsigned short capabilities,
    unsigned short multiword_dma, unsigned char *transfer_mode);
int ata_dma_status_is_complete(unsigned char status);

#endif
