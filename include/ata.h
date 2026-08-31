#ifndef EFESOS_ATA_H
#define EFESOS_ATA_H

#include "../cpu/io.h"

#define ATA_SECTOR_SIZE 512U

void ata_init(void);
int ata_present(void);
unsigned int ata_sector_count(void);
int ata_read_sectors(uint32_t lba, uint8_t count, void *buffer);
int ata_write_sectors(uint32_t lba, uint8_t count, const void *buffer);
uint8_t ata_last_status(void);
uint16_t ata_identify_type(void);

#endif
