#include "ata.h"

#define ATA_DATA 0x1F0U
#define ATA_ERROR 0x1F1U
#define ATA_SECTOR_COUNT 0x1F2U
#define ATA_LBA_LOW 0x1F3U
#define ATA_LBA_MID 0x1F4U
#define ATA_LBA_HIGH 0x1F5U
#define ATA_DRIVE 0x1F6U
#define ATA_STATUS 0x1F7U
#define ATA_COMMAND 0x1F7U
#define ATA_CONTROL 0x3F6U
#define ATA_CMD_IDENTIFY 0xECU
#define ATA_CMD_READ 0x20U
#define ATA_CMD_WRITE 0x30U
#define ATA_CMD_FLUSH 0xE7U
#define ATA_STATUS_ERR 0x01U
#define ATA_STATUS_DRQ 0x08U
#define ATA_STATUS_SRV 0x10U
#define ATA_STATUS_DF 0x20U
#define ATA_STATUS_RDY 0x40U
#define ATA_STATUS_BSY 0x80U
#define ATA_TIMEOUT 1U

static int device_present;
static uint32_t sectors;
static uint8_t last_status;
static uint16_t identify_type;

static unsigned int irq_save(void)
{
    unsigned int flags;
    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void irq_restore(unsigned int flags)
{
    __asm__ volatile ("pushl %0; popfl" : : "r"(flags) : "memory", "cc");
}

static void ata_400ns_delay(void)
{
    inb(ATA_STATUS);
    inb(ATA_STATUS);
    inb(ATA_STATUS);
    inb(ATA_STATUS);
}

static int wait_status(uint8_t required, uint8_t forbidden)
{
    unsigned int attempt;

    for (attempt = 0; attempt < ATA_TIMEOUT; attempt++) {
        uint8_t status = inb(ATA_STATUS);
        last_status = status;

        if ((status & forbidden) != 0U) {
            return 0;
        }
        if ((status & required) == required) {
            return 1;
        }
    }
    return 0;
}

static void select_lba(uint32_t lba, uint8_t count)
{
    outb(ATA_DRIVE, (uint8_t)(0xE0U | ((lba >> 24U) & 0x0FU)));
    ata_400ns_delay();
    outb(ATA_SECTOR_COUNT, count);
    outb(ATA_LBA_LOW, (uint8_t)(lba & 0xFFU));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8U) & 0xFFU));
    outb(ATA_LBA_HIGH, (uint8_t)((lba >> 16U) & 0xFFU));
}

static int valid_request(uint32_t lba, uint8_t count, const void *buffer)
{
    if (!device_present || count == 0U || count > 128U || buffer == 0 ||
        lba >= sectors || (uint32_t)count > sectors - lba) {
        return 0;
    }
    return 1;
}

void ata_init(void)
{
    uint16_t identify[256];
    unsigned int index;
    uint8_t status;

    device_present = 0;
    sectors = 0;
    last_status = 0;
    identify_type = 0;
    /* Polling mode: disable ATA IRQ delivery while commands are in flight. */
    outb(ATA_CONTROL, 0x02U);
    outb(ATA_DRIVE, 0xA0U);
    outb(ATA_SECTOR_COUNT, 0);
    outb(ATA_LBA_LOW, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HIGH, 0);
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);
    status = inb(ATA_STATUS);
    if (status == 0U || !wait_status(ATA_STATUS_DRQ, ATA_STATUS_BSY | ATA_STATUS_ERR | ATA_STATUS_DF)) {
        return;
    }
    for (index = 0; index < 256U; index++) {
        identify[index] = inw(ATA_DATA);
    }
    identify_type = identify[0];
    if ((identify[49] & 0x0200U) == 0U) {
        return;
    }
    sectors = ((uint32_t)identify[61] << 16U) | identify[60];
    device_present = sectors != 0U;
    if (device_present) {
        outb(ATA_CONTROL, 0x06U);
        ata_400ns_delay();
        outb(ATA_CONTROL, 0x02U);
        wait_status(0, ATA_STATUS_BSY);
    }
}

int ata_present(void)
{
    return device_present;
}

unsigned int ata_sector_count(void)
{
    return sectors;
}

int ata_read_sectors(uint32_t lba, uint8_t count, void *buffer)
{
    uint8_t *destination = (uint8_t *)buffer;
    unsigned int sector;
    unsigned int flags;

    if (!valid_request(lba, count, buffer)) {
        return 0;
    }
    flags = irq_save();
    if (!wait_status(0, ATA_STATUS_BSY)) {
        irq_restore(flags);
        return 0;
    }
    for (sector = 0; sector < count; sector++) {
        select_lba(lba + sector, 1);
        outb(ATA_COMMAND, ATA_CMD_READ);
        ata_400ns_delay();
        if (!wait_status(ATA_STATUS_DRQ, ATA_STATUS_BSY | ATA_STATUS_ERR | ATA_STATUS_DF)) {
            irq_restore(flags);
            return 0;
        }
        {
            unsigned int word;
            for (word = 0; word < ATA_SECTOR_SIZE / 2U; word++) {
                uint16_t value = inw(ATA_DATA);
                destination[(sector * ATA_SECTOR_SIZE) + (word * 2U)] = (uint8_t)(value & 0xFFU);
                destination[(sector * ATA_SECTOR_SIZE) + (word * 2U) + 1U] = (uint8_t)(value >> 8U);
            }
        }
    }
    irq_restore(flags);
    return 1;
}

int ata_write_sectors(uint32_t lba, uint8_t count, const void *buffer)
{
    const uint8_t *source = (const uint8_t *)buffer;
    unsigned int sector;
    unsigned int flags;

    if (!valid_request(lba, count, buffer)) {
        return 0;
    }
    flags = irq_save();
    if (!wait_status(0, ATA_STATUS_BSY)) {
        irq_restore(flags);
        return 0;
    }
    for (sector = 0; sector < count; sector++) {
        select_lba(lba + sector, 1);
        outb(ATA_COMMAND, ATA_CMD_WRITE);
        ata_400ns_delay();
        if (!wait_status(ATA_STATUS_DRQ, ATA_STATUS_BSY | ATA_STATUS_ERR | ATA_STATUS_DF)) {
            irq_restore(flags);
            return 0;
        }
        {
            unsigned int word;
            for (word = 0; word < ATA_SECTOR_SIZE / 2U; word++) {
                uint16_t value = source[(sector * ATA_SECTOR_SIZE) + (word * 2U)] |
                    ((uint16_t)source[(sector * ATA_SECTOR_SIZE) + (word * 2U) + 1U] << 8U);
                outw(ATA_DATA, value);
            }
        }
    }
    outb(ATA_COMMAND, ATA_CMD_FLUSH);
    if (!wait_status(0, ATA_STATUS_BSY | ATA_STATUS_ERR | ATA_STATUS_DF)) {
        irq_restore(flags);
        return 0;
    }
    irq_restore(flags);
    return 1;
}

uint8_t ata_last_status(void)
{
    return last_status;
}

uint16_t ata_identify_type(void)
{
    return identify_type;
}
