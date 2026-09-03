#include <stdio.h>
#include <string.h>

#include "ata_dma.h"

static int test_prd_contract(void)
{
    struct ata_dma_prd prd;

    memset(&prd, 0, sizeof(prd));
    if (!ata_dma_prepare_prd(&prd, 0x00102000U, 4096U) ||
        prd.physical_base != 0x00102000U || prd.byte_count != 4096U ||
        prd.flags != ATA_DMA_PRD_END) {
        return 0;
    }
    if (!ata_dma_prepare_prd(&prd, 0x00100000U, 0x10000U) ||
        prd.byte_count != 0U ||
        ata_dma_prepare_prd(&prd, 0x00102001U, 512U) ||
        ata_dma_prepare_prd(&prd, 0x0010F000U, 8192U) ||
        ata_dma_prepare_prd(&prd, 0x00102000U, 0U) ||
        ata_dma_prepare_prd(0, 0x00102000U, 512U)) {
        return 0;
    }
    return 1;
}

static int test_controller_contract(void)
{
    struct pci_device device;
    unsigned short base = 0U;

    memset(&device, 0, sizeof(device));
    device.class_code = 0x01U;
    device.subclass = 0x01U;
    device.prog_if = 0x80U;
    device.bars[4].type = PCI_BAR_IO;
    device.bars[4].flags = 1U;
    device.bars[4].base_low = 0xC000U;
    if (!ata_dma_controller_base(&device, &base) || base != 0xC000U) {
        return 0;
    }
    device.prog_if |= 1U;
    if (ata_dma_controller_base(&device, &base)) {
        return 0;
    }
    device.prog_if = 0x80U;
    device.bars[4].base_low = 0xC004U;
    if (ata_dma_controller_base(&device, &base)) {
        return 0;
    }
    device.bars[4].base_low = 0xC000U;
    device.bars[4].type = PCI_BAR_MEMORY32;
    if (ata_dma_controller_base(&device, &base)) {
        return 0;
    }
    device.bars[4].type = PCI_BAR_IO;
    device.prog_if = 0U;
    return !ata_dma_controller_base(&device, &base);
}

static int test_mode_and_status_contract(void)
{
    unsigned char mode = 0U;

    if (!ata_dma_select_multiword_mode(0x0100U, 0x0007U, &mode) ||
        mode != 0x22U ||
        !ata_dma_select_multiword_mode(0x0100U, 0x0001U, &mode) ||
        mode != 0x20U ||
        ata_dma_select_multiword_mode(0U, 0x0007U, &mode) ||
        ata_dma_select_multiword_mode(0x0100U, 0U, &mode) ||
        ata_dma_select_multiword_mode(0x0100U, 1U, 0)) {
        return 0;
    }
    return ata_dma_status_is_complete(ATA_DMA_STATUS_INTERRUPT) &&
        !ata_dma_status_is_complete(0U) &&
        !ata_dma_status_is_complete(ATA_DMA_STATUS_INTERRUPT |
            ATA_DMA_STATUS_ACTIVE) &&
        !ata_dma_status_is_complete(ATA_DMA_STATUS_INTERRUPT |
            ATA_DMA_STATUS_ERROR);
}

int main(void)
{
    if (!test_prd_contract() || !test_controller_contract() ||
        !test_mode_and_status_contract()) {
        fputs("ATA DMA host self-test failed.\n", stderr);
        return 1;
    }
    puts("ATA DMA host self-test passed.");
    return 0;
}
