#include "ata.h"
#include "ata_dma.h"
#include "ata_irq_state.h"
#include "pci.h"
#include "pmm.h"

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
#define ATA_CMD_SET_FEATURES 0xEFU
#define ATA_CMD_READ 0x20U
#define ATA_CMD_READ_EXT 0x24U
#define ATA_CMD_READ_DMA 0xC8U
#define ATA_CMD_READ_DMA_EXT 0x25U
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
#define ATA_IRQ_WAIT_TIMEOUT 10000U
#define ATA_LBA28_LIMIT 0x10000000U
#define ATA_WRITES_PROTECTED_BY_DEFAULT 1
#define ATA_SET_TRANSFER_MODE 0x03U
#define ATA_DMA_BOUNCE_BYTES PMM_BLOCK_SIZE
#define ATA_DMA_MAX_SECTORS (ATA_DMA_BOUNCE_BYTES / ATA_SECTOR_SIZE)
#define ATA_DMA_MEMORY_LIMIT 0x00400000U
#define ATA_BUS_MASTER_COMMAND 0U
#define ATA_BUS_MASTER_STATUS 2U
#define ATA_BUS_MASTER_PRDT 4U
#define ATA_BUS_MASTER_START 0x01U
#define ATA_BUS_MASTER_READ_FROM_DISK 0x08U
#define ATA_BUS_MASTER_DRIVE0_CAPABLE 0x20U

static int device_present;
static uint32_t sectors;
static uint8_t last_status;
static uint16_t identify_type;
static uint16_t identify_capabilities;
static uint16_t identify_multiword_dma;
static int lba48_supported;
static int writes_protected;
static uint32_t write_window_start;
static uint32_t write_window_sectors;
static struct block_device primary_block_device;
static struct ata_irq_state irq_state;
static volatile int request_active;
static const struct pci_device *dma_controller;
static uint16_t dma_io_base;
static uint32_t dma_bounce_physical;
static uint32_t dma_prdt_physical;
static uint8_t dma_transfer_mode;
static int dma_enabled;
static unsigned int dma_transfers;
static unsigned int dma_fallbacks;

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

static int cpu_interrupts_enabled(void)
{
    unsigned int flags;

    __asm__ volatile ("pushfl; popl %0" : "=r"(flags) : : "memory");
    return (flags & (1U << 9U)) != 0U;
}

static void dma_memory_barrier(void)
{
    __asm__ volatile ("" : : : "memory");
}

static int begin_request(void)
{
    unsigned int flags = irq_save();

    if (request_active) {
        irq_restore(flags);
        return 0;
    }
    request_active = 1;
    irq_restore(flags);
    return 1;
}

static void end_request(void)
{
    unsigned int flags = irq_save();

    request_active = 0;
    irq_restore(flags);
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

static int wait_status_with_irq(uint8_t required, uint8_t forbidden,
    unsigned int snapshot)
{
    unsigned int attempt;

    if (ata_irq_state_is_enabled(&irq_state) && cpu_interrupts_enabled()) {
        for (attempt = 0U; attempt < ATA_IRQ_WAIT_TIMEOUT; attempt++) {
            int observation = ata_irq_state_observe(&irq_state, snapshot,
                required, (uint8_t)(forbidden & (uint8_t)~ATA_STATUS_BSY),
                ATA_STATUS_BSY);

            if (observation == ATA_IRQ_OBSERVE_READY) {
                uint8_t current_status = inb(ATA_CONTROL);

                last_status = current_status;
                if ((current_status & (forbidden & (uint8_t)~ATA_STATUS_BSY)) != 0U) {
                    return 0;
                }
                if ((current_status & ATA_STATUS_BSY) == 0U &&
                    (current_status & required) == required) {
                    return 1;
                }
                break;
            }
            if (observation == ATA_IRQ_OBSERVE_ERROR) {
                last_status = (uint8_t)ata_irq_state_status(&irq_state);
                return 0;
            }
            if (observation == ATA_IRQ_OBSERVE_PENDING) {
                break;
            }
            __asm__ volatile ("pause" : : : "memory");
        }
        ata_irq_state_record_fallback(&irq_state);
    }
    return wait_status(required, forbidden);
}

static void ata_soft_reset(void)
{
    uint8_t control = ata_irq_state_is_enabled(&irq_state) ? 0U : 0x02U;

    outb(ATA_CONTROL, control | ATA_CONTROL_SOFT_RESET);
    ata_400ns_delay();
    outb(ATA_CONTROL, control);
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

static const struct pci_device *find_dma_controller(uint16_t *io_base)
{
    const struct pci_device *match = 0;
    unsigned int index;

    for (index = 0U; index < pci_device_count(); index++) {
        const struct pci_device *candidate = pci_device_at(index);
        uint16_t candidate_base;

        if (!ata_dma_controller_base(candidate, &candidate_base)) {
            continue;
        }
        /* Two controllers claiming the legacy primary channel are ambiguous.
           Refuse DMA rather than programming the wrong bus-master engine. */
        if (match != 0) {
            return 0;
        }
        match = candidate;
        *io_base = candidate_base;
    }
    return match;
}

static void stop_dma_engine(void)
{
    uint8_t command;
    uint8_t status;

    if (dma_io_base == 0U) {
        return;
    }
    command = inb((uint16_t)(dma_io_base + ATA_BUS_MASTER_COMMAND));
    outb((uint16_t)(dma_io_base + ATA_BUS_MASTER_COMMAND),
        (uint8_t)(command & (uint8_t)~ATA_BUS_MASTER_START));
    status = inb((uint16_t)(dma_io_base + ATA_BUS_MASTER_STATUS));
    outb((uint16_t)(dma_io_base + ATA_BUS_MASTER_STATUS),
        (uint8_t)(status | ATA_DMA_STATUS_ERROR |
            ATA_DMA_STATUS_INTERRUPT));
}

static void disable_dma_after_failure(void)
{
    const struct pci_device *controller = dma_controller;
    uint32_t bounce = dma_bounce_physical;
    uint32_t prdt = dma_prdt_physical;

    stop_dma_engine();
    if (dma_enabled) {
        dma_enabled = 0;
        dma_fallbacks++;
    }
    if (controller != 0) {
        pci_disable_ide_bus_master(controller);
    }
    dma_controller = 0;
    dma_io_base = 0U;
    dma_bounce_physical = 0U;
    dma_prdt_physical = 0U;
    dma_transfer_mode = 0U;
    if (prdt != 0U) {
        pmm_free_block(prdt);
    }
    if (bounce != 0U) {
        pmm_free_block(bounce);
    }
}

static int set_dma_transfer_mode(uint8_t transfer_mode)
{
    unsigned int irq_snapshot;

    if (!wait_status(0U, ATA_STATUS_BSY)) {
        return 0;
    }
    outb(ATA_DRIVE, 0xE0U);
    ata_400ns_delay();
    outb(ATA_ERROR, ATA_SET_TRANSFER_MODE);
    outb(ATA_SECTOR_COUNT, transfer_mode);
    irq_snapshot = ata_irq_state_snapshot(&irq_state);
    outb(ATA_COMMAND, ATA_CMD_SET_FEATURES);
    ata_400ns_delay();
    return wait_status_with_irq(0U,
        ATA_STATUS_BSY | ATA_STATUS_ERR | ATA_STATUS_DF, irq_snapshot);
}

static int ata_read_dma_chunk(uint32_t lba, uint8_t count,
    uint8_t *destination)
{
    struct ata_dma_prd *prd = (struct ata_dma_prd *)dma_prdt_physical;
    uint8_t *bounce = (uint8_t *)dma_bounce_physical;
    unsigned int byte_count = (unsigned int)count * ATA_SECTOR_SIZE;
    unsigned int index;
    unsigned int irq_snapshot;
    uint8_t bus_master_status;
    int completed;

    if (!ata_dma_prepare_prd(prd, dma_bounce_physical, byte_count)) {
        return 0;
    }
    for (index = 0U; index < byte_count; index++) {
        bounce[index] = 0U;
    }
    dma_memory_barrier();
    if (!wait_status(0U, ATA_STATUS_BSY)) {
        return 0;
    }

    stop_dma_engine();
    outl((uint16_t)(dma_io_base + ATA_BUS_MASTER_PRDT), dma_prdt_physical);
    outb((uint16_t)(dma_io_base + ATA_BUS_MASTER_STATUS),
        (uint8_t)(inb((uint16_t)(dma_io_base + ATA_BUS_MASTER_STATUS)) |
            ATA_BUS_MASTER_DRIVE0_CAPABLE | ATA_DMA_STATUS_ERROR |
            ATA_DMA_STATUS_INTERRUPT));
    outb((uint16_t)(dma_io_base + ATA_BUS_MASTER_COMMAND),
        ATA_BUS_MASTER_READ_FROM_DISK);
    if (!select_request(lba, count)) {
        return 0;
    }
    irq_snapshot = ata_irq_state_snapshot(&irq_state);
    outb(ATA_COMMAND, request_needs_lba48(lba, count) ?
        ATA_CMD_READ_DMA_EXT : ATA_CMD_READ_DMA);
    ata_400ns_delay();
    outb((uint16_t)(dma_io_base + ATA_BUS_MASTER_COMMAND),
        ATA_BUS_MASTER_READ_FROM_DISK | ATA_BUS_MASTER_START);

    completed = wait_status_with_irq(0U,
        ATA_STATUS_BSY | ATA_STATUS_ERR | ATA_STATUS_DF, irq_snapshot);
    bus_master_status = inb((uint16_t)(dma_io_base + ATA_BUS_MASTER_STATUS));
    outb((uint16_t)(dma_io_base + ATA_BUS_MASTER_COMMAND),
        ATA_BUS_MASTER_READ_FROM_DISK);
    outb((uint16_t)(dma_io_base + ATA_BUS_MASTER_STATUS),
        (uint8_t)(bus_master_status | ATA_DMA_STATUS_ERROR |
            ATA_DMA_STATUS_INTERRUPT));
    if (!completed || !ata_dma_status_is_complete(bus_master_status)) {
        return 0;
    }
    dma_memory_barrier();
    for (index = 0U; index < byte_count; index++) {
        destination[index] = bounce[index];
    }
    dma_transfers++;
    return 1;
}

static int ata_read_dma_once(uint32_t lba, uint8_t count, void *buffer)
{
    uint8_t *destination = (uint8_t *)buffer;
    unsigned int remaining = count;
    unsigned int completed_sectors = 0U;

    while (remaining != 0U) {
        uint8_t chunk = remaining > ATA_DMA_MAX_SECTORS ?
            ATA_DMA_MAX_SECTORS : (uint8_t)remaining;

        if (!ata_read_dma_chunk(lba + completed_sectors, chunk,
            destination + completed_sectors * ATA_SECTOR_SIZE)) {
            return 0;
        }
        completed_sectors += chunk;
        remaining -= chunk;
    }
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

    if (dma_io_base != 0U || dma_bounce_physical != 0U ||
        dma_prdt_physical != 0U) {
        disable_dma_after_failure();
    }
    device_present = 0;
    sectors = 0;
    last_status = 0;
    identify_type = 0;
    identify_capabilities = 0U;
    identify_multiword_dma = 0U;
    lba48_supported = 0;
    writes_protected = ATA_WRITES_PROTECTED_BY_DEFAULT;
    write_window_start = 0U;
    write_window_sectors = 0U;
    block_device_reset(&primary_block_device);
    ata_irq_state_reset(&irq_state);
    request_active = 0;
    dma_controller = 0;
    dma_io_base = 0U;
    dma_bounce_physical = 0U;
    dma_prdt_physical = 0U;
    dma_transfer_mode = 0U;
    dma_enabled = 0;
    dma_transfers = 0U;
    dma_fallbacks = 0U;
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
    identify_capabilities = identify[49];
    identify_multiword_dma = identify[63];
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

static int ata_read_pio_once(uint32_t lba, uint8_t count, void *buffer)
{
    uint8_t *destination = (uint8_t *)buffer;
    unsigned int sector;
    unsigned int irq_snapshot;

    if (!wait_status(0, ATA_STATUS_BSY)) {
        return 0;
    }
    /* Use one bounded multi-sector PIO command. Re-selecting the device for
       every sector is needlessly slow and makes journal validation depend on
       hundreds of controller state transitions. */
    if (!select_request(lba, count)) {
        return 0;
    }
    irq_snapshot = ata_irq_state_snapshot(&irq_state);
    outb(ATA_COMMAND, request_needs_lba48(lba, count) ? ATA_CMD_READ_EXT : ATA_CMD_READ);
    ata_400ns_delay();
    for (sector = 0; sector < count; sector++) {
        if (!wait_status_with_irq(ATA_STATUS_DRQ,
            ATA_STATUS_BSY | ATA_STATUS_ERR | ATA_STATUS_DF, irq_snapshot)) {
            return 0;
        }
        irq_snapshot = ata_irq_state_snapshot(&irq_state);
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
    unsigned int attempt;

    if (!valid_request(lba, count, buffer)) {
        return 0;
    }
    if (!begin_request()) {
        return 0;
    }
    for (attempt = 0U; attempt < 3U; attempt++) {
        if ((dma_enabled ? ata_read_dma_once(lba, count, buffer) :
                ata_read_pio_once(lba, count, buffer))) {
            end_request();
            return 1;
        }
        if (dma_enabled) {
            disable_dma_after_failure();
        }
        if (attempt + 1U < 3U) {
            ata_soft_reset();
        }
    }
    end_request();
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
    unsigned int irq_snapshot;
    int success = 0;

    if (writes_protected || !valid_request(lba, count, buffer) ||
        lba < write_window_start ||
        lba - write_window_start >= write_window_sectors ||
        (uint32_t)count > write_window_sectors - (lba - write_window_start)) {
        return 0;
    }
    if (!begin_request()) {
        return 0;
    }
    if (!wait_status(0, ATA_STATUS_BSY)) {
        goto done;
    }
    for (sector = 0; sector < count; sector++) {
        if (!select_request(lba + sector, 1)) {
            goto done;
        }
        irq_snapshot = ata_irq_state_snapshot(&irq_state);
        outb(ATA_COMMAND, request_needs_lba48(lba + sector, 1) ?
            ATA_CMD_WRITE_EXT : ATA_CMD_WRITE);
        ata_400ns_delay();
        if (!wait_status_with_irq(ATA_STATUS_DRQ,
            ATA_STATUS_BSY | ATA_STATUS_ERR | ATA_STATUS_DF, irq_snapshot)) {
            goto done;
        }
        irq_snapshot = ata_irq_state_snapshot(&irq_state);
        {
            unsigned int word;
            for (word = 0; word < ATA_SECTOR_SIZE / 2U; word++) {
                uint16_t value = source[(sector * ATA_SECTOR_SIZE) + (word * 2U)] |
                    ((uint16_t)source[(sector * ATA_SECTOR_SIZE) + (word * 2U) + 1U] << 8U);
                outw(ATA_DATA, value);
            }
        }
        if (!wait_status_with_irq(0,
            ATA_STATUS_BSY | ATA_STATUS_ERR | ATA_STATUS_DF, irq_snapshot)) {
            goto done;
        }
    }
    irq_snapshot = ata_irq_state_snapshot(&irq_state);
    outb(ATA_COMMAND, lba48_supported ? ATA_CMD_FLUSH_EXT : ATA_CMD_FLUSH);
    if (!wait_status_with_irq(0,
        ATA_STATUS_BSY | ATA_STATUS_ERR | ATA_STATUS_DF, irq_snapshot)) {
        goto done;
    }
    success = 1;
done:
    end_request();
    return success;
}

void ata_irq_handler(void)
{
    uint8_t status = inb(ATA_STATUS);

    last_status = status;
    ata_irq_state_record(&irq_state, status);
}

int ata_enable_irq_mode(void)
{
    unsigned int flags;

    if (!device_present) {
        return 0;
    }
    flags = irq_save();
    if (request_active) {
        irq_restore(flags);
        return 0;
    }
    (void)inb(ATA_STATUS);
    ata_irq_state_enable(&irq_state);
    outb(ATA_CONTROL, 0U);
    ata_400ns_delay();
    irq_restore(flags);
    return 1;
}

int ata_irq_mode_enabled(void)
{
    return ata_irq_state_is_enabled(&irq_state);
}

int ata_enable_dma_mode(void)
{
    const struct pci_device *controller;
    uint16_t io_base = 0U;
    uint8_t transfer_mode = 0U;
    uint32_t bounce;
    uint32_t prdt;
    uint8_t status;
    int success = 0;

    if (!device_present || !ata_irq_state_is_enabled(&irq_state) ||
        dma_enabled ||
        !ata_dma_select_multiword_mode(identify_capabilities,
            identify_multiword_dma, &transfer_mode)) {
        return 0;
    }
    controller = find_dma_controller(&io_base);
    if (controller == 0 || !begin_request()) {
        return 0;
    }

    bounce = pmm_alloc_block_below(ATA_DMA_MEMORY_LIMIT);
    prdt = pmm_alloc_block_below(ATA_DMA_MEMORY_LIMIT);
    if (bounce == 0U || prdt == 0U ||
        !ata_dma_prepare_prd((struct ata_dma_prd *)prdt, bounce,
            ATA_DMA_BOUNCE_BYTES)) {
        goto done;
    }
    if (!pci_enable_ide_bus_master(controller)) {
        (void)pci_disable_ide_bus_master(controller);
        goto done;
    }

    dma_controller = controller;
    dma_io_base = io_base;
    dma_bounce_physical = bounce;
    dma_prdt_physical = prdt;
    dma_transfer_mode = transfer_mode;
    stop_dma_engine();
    if (!set_dma_transfer_mode(transfer_mode)) {
        pci_disable_ide_bus_master(controller);
        ata_soft_reset();
        dma_controller = 0;
        dma_io_base = 0U;
        dma_bounce_physical = 0U;
        dma_prdt_physical = 0U;
        dma_transfer_mode = 0U;
        goto done;
    }
    status = inb((uint16_t)(dma_io_base + ATA_BUS_MASTER_STATUS));
    outb((uint16_t)(dma_io_base + ATA_BUS_MASTER_STATUS),
        (uint8_t)(status | ATA_BUS_MASTER_DRIVE0_CAPABLE |
            ATA_DMA_STATUS_ERROR | ATA_DMA_STATUS_INTERRUPT));
    dma_enabled = 1;
    success = 1;

done:
    if (!success) {
        if (prdt != 0U) {
            pmm_free_block(prdt);
        }
        if (bounce != 0U) {
            pmm_free_block(bounce);
        }
    }
    end_request();
    return success;
}

int ata_dma_mode_enabled(void)
{
    return dma_enabled;
}

unsigned int ata_dma_transfer_mode(void)
{
    return dma_transfer_mode;
}

unsigned int ata_dma_transfer_count(void)
{
    return dma_transfers;
}

unsigned int ata_dma_fallback_count(void)
{
    return dma_fallbacks;
}

unsigned int ata_irq_count(void)
{
    return ata_irq_state_count(&irq_state);
}

unsigned int ata_irq_fallback_count(void)
{
    return ata_irq_state_fallback_count(&irq_state);
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
