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
#define ATA_CMD_READ_EXT 0x24U
#define ATA_CMD_WRITE 0x30U
#define ATA_CMD_WRITE_EXT 0x34U
#define ATA_CMD_FLUSH 0xE7U
#define ATA_CMD_FLUSH_EXT 0xEAU
#define ATA_CONTROL_SOFT_RESET 0x04U
#define ATA_STATUS_ERR 0x01U
#define ATA_STATUS_DRQ 0x08U
#define ATA_STATUS_SRV 0x10U
#define ATA_STATUS_DF 0x20U
#define ATA_STATUS_RDY 0x40U
#define ATA_STATUS_BSY 0x80U
#define ATA_TIMEOUT 100000U
#define ATA_LBA28_LIMIT 0x10000000U
#define ATA_WRITES_PROTECTED_BY_DEFAULT 1

static int device_present;
static uint32_t sectors;
static uint8_t last_status;
static uint16_t identify_type;
static int lba48_supported;
static int writes_protected;
static uint32_t write_window_start;
static uint32_t write_window_sectors;
static struct block_device primary_block_device;

static int ata_block_read(void *context, unsigned int lba,
    unsigned char count, void *buffer)
{
    (void)context;
    return ata_read_sectors(lba, count, buffer);
}

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

static void ata_poll_delay(void)
{
    inb(ATA_CONTROL);
    inb(ATA_CONTROL);
    inb(ATA_CONTROL);
    inb(ATA_CONTROL);
}

static int wait_status(uint8_t required, uint8_t forbidden)
{
    unsigned int attempt;

    for (attempt = 0; attempt < ATA_TIMEOUT; attempt++) {
        uint8_t status = inb(ATA_STATUS);
        last_status = status;

        if ((status & (forbidden & (uint8_t)~ATA_STATUS_BSY)) != 0U) {
            return 0;
        }
        if ((status & ATA_STATUS_BSY) != 0U) {
            ata_poll_delay();
            continue;
        }
        if ((status & required) == required) {
            return 1;
        }
        ata_poll_delay();
    }
    return 0;
}

static void ata_soft_reset(void)
{
    /* Keep IRQ delivery disabled while resetting the polling-mode channel. */
    outb(ATA_CONTROL, 0x02U | ATA_CONTROL_SOFT_RESET);
    ata_400ns_delay();
    outb(ATA_CONTROL, 0x02U);
    ata_400ns_delay();
    wait_status(0, ATA_STATUS_BSY);
}

static int request_needs_lba48(uint32_t lba, uint8_t count)
{
    return lba >= ATA_LBA28_LIMIT || (uint32_t)count > ATA_LBA28_LIMIT - lba;
}

static void select_lba28(uint32_t lba, uint8_t count)
{
    outb(ATA_DRIVE, (uint8_t)(0xE0U | ((lba >> 24U) & 0x0FU)));
    ata_400ns_delay();
    outb(ATA_SECTOR_COUNT, count);
    outb(ATA_LBA_LOW, (uint8_t)(lba & 0xFFU));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8U) & 0xFFU));
    outb(ATA_LBA_HIGH, (uint8_t)((lba >> 16U) & 0xFFU));
}

static void select_lba48(uint32_t lba, uint8_t count)
{
    /* ATA-6 requires the high-order task-file bytes before the low-order
       bytes. The 32-bit public LBA API leaves the upper 16 bits zero. */
    outb(ATA_DRIVE, 0xE0U);
    ata_400ns_delay();
    outb(ATA_SECTOR_COUNT, 0U);
    outb(ATA_LBA_LOW, (uint8_t)((lba >> 24U) & 0xFFU));
    outb(ATA_LBA_MID, 0U);
    outb(ATA_LBA_HIGH, 0U);
    outb(ATA_SECTOR_COUNT, count);
    outb(ATA_LBA_LOW, (uint8_t)(lba & 0xFFU));
    outb(ATA_LBA_MID, (uint8_t)((lba >> 8U) & 0xFFU));
    outb(ATA_LBA_HIGH, (uint8_t)((lba >> 16U) & 0xFFU));
}

static int select_request(uint32_t lba, uint8_t count)
{
    if (request_needs_lba48(lba, count)) {
        if (!lba48_supported) {
            return 0;
        }
        select_lba48(lba, count);
        return 1;
    }
    select_lba28(lba, count);
    return 1;
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
    lba48_supported = 0;
    writes_protected = ATA_WRITES_PROTECTED_BY_DEFAULT;
    write_window_start = 0U;
    write_window_sectors = 0U;
    block_device_reset(&primary_block_device);
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
    if ((identify[83] & 0x0400U) != 0U) {
        /* Keep the public 32-bit LBA contract explicit. A device larger than
           that cannot be addressed safely by this API and is rejected. */
        if (identify[103] != 0U || identify[102] != 0U) {
            sectors = 0U;
            return;
        }
        sectors = ((uint32_t)identify[101] << 16U) | identify[100];
        lba48_supported = sectors != 0U;
    } else {
        sectors = ((uint32_t)identify[61] << 16U) | identify[60];
        if (sectors == 0U || sectors > ATA_LBA28_LIMIT) {
            sectors = 0U;
            return;
        }
    }
    if (sectors == 0U) {
        return;
    }
    device_present = 1;
    if (device_present) {
        outb(ATA_CONTROL, 0x06U);
        ata_400ns_delay();
        outb(ATA_CONTROL, 0x02U);
        wait_status(0, ATA_STATUS_BSY);
    }
    if (!block_device_configure(&primary_block_device, sectors,
        ATA_SECTOR_SIZE, 128U, ata_block_read, 0, 0)) {
        device_present = 0;
        sectors = 0U;
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

static int ata_read_sectors_once(uint32_t lba, uint8_t count, void *buffer)
{
    uint8_t *destination = (uint8_t *)buffer;
    unsigned int sector;

    if (!wait_status(0, ATA_STATUS_BSY)) {
        return 0;
    }
    /* Use one bounded multi-sector PIO command. Re-selecting the device for
       every sector is needlessly slow and makes journal validation depend on
       hundreds of controller state transitions. */
    if (!select_request(lba, count)) {
        return 0;
    }
    outb(ATA_COMMAND, request_needs_lba48(lba, count) ? ATA_CMD_READ_EXT : ATA_CMD_READ);
    ata_400ns_delay();
    for (sector = 0; sector < count; sector++) {
        if (!wait_status(ATA_STATUS_DRQ, ATA_STATUS_BSY | ATA_STATUS_ERR | ATA_STATUS_DF)) {
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
    if (!wait_status(0, ATA_STATUS_BSY | ATA_STATUS_ERR | ATA_STATUS_DF)) {
        return 0;
    }
    return 1;
}

int ata_read_sectors(uint32_t lba, uint8_t count, void *buffer)
{
    unsigned int flags;
    unsigned int attempt;

    if (!valid_request(lba, count, buffer)) {
        return 0;
    }
    flags = irq_save();
    for (attempt = 0U; attempt < 3U; attempt++) {
        if (ata_read_sectors_once(lba, count, buffer)) {
            irq_restore(flags);
            return 1;
        }
        if (attempt + 1U < 3U) {
            ata_soft_reset();
        }
    }
    irq_restore(flags);
    return 0;
}

int ata_write_protected(void)
{
    return writes_protected;
}

int ata_enable_transactional_writes(uint32_t start_lba, uint32_t sector_count)
{
    if (!device_present || sector_count == 0U || start_lba >= sectors ||
        sector_count > sectors - start_lba) {
        return 0;
    }
    write_window_start = start_lba;
    write_window_sectors = sector_count;
    writes_protected = 0;
    return 1;
}

void ata_disable_transactional_writes(void)
{
    write_window_start = 0U;
    write_window_sectors = 0U;
    writes_protected = 1;
}

int ata_write_sectors(uint32_t lba, uint8_t count, const void *buffer)
{
    const uint8_t *source = (const uint8_t *)buffer;
    unsigned int sector;
    unsigned int flags;

    if (writes_protected || !valid_request(lba, count, buffer) ||
        lba < write_window_start ||
        lba - write_window_start >= write_window_sectors ||
        (uint32_t)count > write_window_sectors - (lba - write_window_start)) {
        return 0;
    }
    flags = irq_save();
    if (!wait_status(0, ATA_STATUS_BSY)) {
        irq_restore(flags);
        return 0;
    }
    for (sector = 0; sector < count; sector++) {
        if (!select_request(lba + sector, 1)) {
            irq_restore(flags);
            return 0;
        }
        outb(ATA_COMMAND, request_needs_lba48(lba + sector, 1) ?
            ATA_CMD_WRITE_EXT : ATA_CMD_WRITE);
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
        if (!wait_status(0, ATA_STATUS_BSY | ATA_STATUS_ERR | ATA_STATUS_DF)) {
            irq_restore(flags);
            return 0;
        }
    }
    outb(ATA_COMMAND, lba48_supported ? ATA_CMD_FLUSH_EXT : ATA_CMD_FLUSH);
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

int ata_lba48_supported(void)
{
    return lba48_supported;
}

const struct block_device *ata_block_device(void)
{
    return block_device_is_ready(&primary_block_device) ?
        &primary_block_device : 0;
}
