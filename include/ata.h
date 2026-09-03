#ifndef EFESOS_ATA_H
#define EFESOS_ATA_H

#include "../cpu/io.h"
#include "block_device.h"

#define ATA_SECTOR_SIZE 512U

void ata_init(void);
void ata_irq_handler(void);
int ata_enable_irq_mode(void);
int ata_irq_mode_enabled(void);
unsigned int ata_irq_count(void);
unsigned int ata_irq_fallback_count(void);
int ata_enable_dma_mode(void);
int ata_dma_mode_enabled(void);
unsigned int ata_dma_transfer_mode(void);
unsigned int ata_dma_transfer_count(void);
unsigned int ata_dma_fallback_count(void);
int ata_present(void);
unsigned int ata_sector_count(void);
int ata_read_sectors(uint32_t lba, uint8_t count, void *buffer);
int ata_write_sectors(uint32_t lba, uint8_t count, const void *buffer);
int ata_enable_transactional_writes(uint32_t start_lba, uint32_t sector_count);
void ata_disable_transactional_writes(void);
int ata_write_protected(void);
uint8_t ata_last_status(void);
uint16_t ata_identify_type(void);
int ata_lba48_supported(void);
const struct block_device *ata_block_device(void);

#endif
