#include "pci.h"
#include "pci_msi.h"

#define PCI_CONFIG_ADDRESS 0x0CF8U
#define PCI_CONFIG_DATA 0x0CFCU

static struct pci_device devices[PCI_MAX_DEVICES];
static unsigned int device_count;
static struct pci_msi_saved_state ahci_msi_state;
static uint8_t ahci_msi_bus;
static uint8_t ahci_msi_slot;
static uint8_t ahci_msi_function;

struct pci_msi_device_context {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
};

static unsigned int interrupt_save(void)
{
    unsigned int flags;

    __asm__ volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void interrupt_restore(unsigned int flags)
{
    if ((flags & (1U << 9U)) != 0U) {
        __asm__ volatile("sti" : : : "memory");
    }
}

static uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset)
{
    uint32_t address = 0x80000000U |
        ((uint32_t)bus << 16U) |
        ((uint32_t)slot << 11U) |
        ((uint32_t)function << 8U) |
        ((uint32_t)offset & 0xFCU);

    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset)
{
    uint32_t value = pci_config_read(bus, slot, function, offset);

    return (uint16_t)((value >> ((offset & 2U) * 8U)) & 0xFFFFU);
}

static void pci_config_write_word(uint8_t bus, uint8_t slot,
    uint8_t function, uint8_t offset, uint16_t value)
{
    uint32_t address = 0x80000000U |
        ((uint32_t)bus << 16U) |
        ((uint32_t)slot << 11U) |
        ((uint32_t)function << 8U) |
        ((uint32_t)offset & 0xFCU);

    outl(PCI_CONFIG_ADDRESS, address);
    outw((uint16_t)(PCI_CONFIG_DATA + (offset & 2U)), value);
}

static void pci_config_write_dword(uint8_t bus, uint8_t slot,
    uint8_t function, uint8_t offset, uint32_t value)
{
    uint32_t address = 0x80000000U |
        ((uint32_t)bus << 16U) |
        ((uint32_t)slot << 11U) |
        ((uint32_t)function << 8U) |
        ((uint32_t)offset & 0xFCU);

    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

static uint16_t pci_msi_read_word(void *context, uint8_t offset)
{
    const struct pci_msi_device_context *device =
        (const struct pci_msi_device_context *)context;

    return pci_config_read_word(device->bus, device->slot,
        device->function, offset);
}

static uint32_t pci_msi_read_dword(void *context, uint8_t offset)
{
    const struct pci_msi_device_context *device =
        (const struct pci_msi_device_context *)context;

    return pci_config_read(device->bus, device->slot,
        device->function, offset);
}

static void pci_msi_write_word(void *context, uint8_t offset, uint16_t value)
{
    const struct pci_msi_device_context *device =
        (const struct pci_msi_device_context *)context;

    pci_config_write_word(device->bus, device->slot,
        device->function, offset, value);
}

static void pci_msi_write_dword(void *context, uint8_t offset, uint32_t value)
{
    const struct pci_msi_device_context *device =
        (const struct pci_msi_device_context *)context;

    pci_config_write_dword(device->bus, device->slot,
        device->function, offset, value);
}

static struct pci_msi_register_io pci_msi_io(
    struct pci_msi_device_context *context)
{
    struct pci_msi_register_io io;

    io.context = context;
    io.read_word = pci_msi_read_word;
    io.read_dword = pci_msi_read_dword;
    io.write_word = pci_msi_write_word;
    io.write_dword = pci_msi_write_dword;
    return io;
}

static void pci_read_configuration(const struct pci_device *device,
    uint8_t *configuration)
{
    unsigned int offset;

    for (offset = 0U; offset < 256U; offset += 4U) {
        uint32_t value = pci_config_read(device->bus, device->slot,
            device->function, (uint8_t)offset);

        configuration[offset] = (uint8_t)value;
        configuration[offset + 1U] = (uint8_t)(value >> 8U);
        configuration[offset + 2U] = (uint8_t)(value >> 16U);
        configuration[offset + 3U] = (uint8_t)(value >> 24U);
    }
}

static int pci_is_present(uint8_t bus, uint8_t slot, uint8_t function)
{
    return pci_config_read_word(bus, slot, function, 0) != 0xFFFFU;
}

static int pci_device_is_recorded(const struct pci_device *device)
{
    unsigned int index;

    if (device == 0) {
        return 0;
    }
    for (index = 0U; index < device_count; index++) {
        if (device == &devices[index]) {
            return 1;
        }
    }
    return 0;
}

static void pci_record(uint8_t bus, uint8_t slot, uint8_t function)
{
    uint32_t class_register;
    uint32_t header_register;
    struct pci_device *device;
    unsigned int bar_index;

    if (device_count == PCI_MAX_DEVICES || !pci_is_present(bus, slot, function)) {
        return;
    }

    device = &devices[device_count++];
    class_register = pci_config_read(bus, slot, function, 8);
    header_register = pci_config_read(bus, slot, function, 0x0C);
    device->bus = bus;
    device->slot = slot;
    device->function = function;
    device->vendor_id = pci_config_read_word(bus, slot, function, 0);
    device->device_id = pci_config_read_word(bus, slot, function, 2);
    device->revision = (uint8_t)(class_register & 0xFFU);
    device->prog_if = (uint8_t)((class_register >> 8U) & 0xFFU);
    device->subclass = (uint8_t)((class_register >> 16U) & 0xFFU);
    device->class_code = (uint8_t)((class_register >> 24U) & 0xFFU);
    device->interrupt_line = (uint8_t)(pci_config_read(bus, slot, function, 0x3C) & 0xFFU);
    device->header_type = (uint8_t)((header_register >> 16U) & 0xFFU);
    for (bar_index = 0U; bar_index < PCI_MAX_BARS; bar_index++) {
        uint32_t raw = pci_config_read(bus, slot, function,
            (uint8_t)(0x10U + bar_index * 4U));
        struct pci_bar *bar = &device->bars[bar_index];

        bar->base_low = 0U;
        bar->base_high = 0U;
        bar->flags = 0U;
        bar->type = PCI_BAR_UNUSED;
        /* BARs are defined for type-0 endpoints. Bridges have a different
           header layout and must not be interpreted as endpoint BARs. */
        if ((device->header_type & 0x7FU) != 0U || raw == 0U || raw == 0xFFFFFFFFU) {
            continue;
        }
        bar->flags = raw & 0x0FU;
        if ((raw & 1U) != 0U) {
            bar->base_low = raw & ~0x3U;
            bar->type = PCI_BAR_IO;
            continue;
        }
        bar->base_low = raw & ~0x0FU;
        if (((raw >> 1U) & 3U) == 2U) {
            if (bar_index + 1U >= PCI_MAX_BARS) {
                /* A truncated 64-bit pair is malformed; do not expose a
                   guessed 32-bit resource to a future driver. */
                bar->base_low = 0U;
                bar->flags = 0U;
                continue;
            }
            bar->base_high = pci_config_read(bus, slot, function,
                (uint8_t)(0x10U + (bar_index + 1U) * 4U));
            bar->type = PCI_BAR_MEMORY64;
            bar_index++;
        } else {
            bar->type = PCI_BAR_MEMORY32;
        }
    }
}

void pci_init(void)
{
    unsigned int bus;
    unsigned int slot;

    device_count = 0;
    for (bus = 0; bus < 256U && device_count < PCI_MAX_DEVICES; bus++) {
        for (slot = 0; slot < 32U && device_count < PCI_MAX_DEVICES; slot++) {
            uint8_t function;
            uint8_t header_type;

            if (!pci_is_present((uint8_t)bus, (uint8_t)slot, 0)) {
                continue;
            }
            pci_record((uint8_t)bus, (uint8_t)slot, 0);
            header_type = (uint8_t)((pci_config_read((uint8_t)bus, (uint8_t)slot, 0, 0x0C) >> 16U) & 0xFFU);
            if ((header_type & 0x80U) == 0U) {
                continue;
            }
            for (function = 1; function < 8U && device_count < PCI_MAX_DEVICES; function++) {
                pci_record((uint8_t)bus, (uint8_t)slot, function);
            }
        }
    }
}

int pci_self_test(void)
{
    unsigned int device_index;

    for (device_index = 0U; device_index < device_count; device_index++) {
        const struct pci_device *device = &devices[device_index];
        unsigned int bar_index;

        if (device->vendor_id == 0xFFFFU ||
            (device->header_type & 0x7FU) > 2U) {
            return 0;
        }
        for (bar_index = 0U; bar_index < PCI_MAX_BARS; bar_index++) {
            const struct pci_bar *bar = &device->bars[bar_index];

            if (bar->type == PCI_BAR_UNUSED) {
                continue;
            }
            if (bar->type == PCI_BAR_IO) {
                if ((bar->base_low & 0x3U) != 0U) {
                    return 0;
                }
            } else if (bar->type == PCI_BAR_MEMORY32 ||
                bar->type == PCI_BAR_MEMORY64) {
                if ((bar->base_low & 0xFU) != 0U ||
                    (bar->type == PCI_BAR_MEMORY64 && bar->base_high == 0xFFFFFFFFU)) {
                    return 0;
                }
            } else {
                return 0;
            }
        }
    }
    return 1;
}

int pci_enable_ide_bus_master(const struct pci_device *device)
{
    unsigned int index;
    uint16_t command;

    if (device == 0 || (device->header_type & 0x7FU) != 0U ||
        device->class_code != 0x01U || device->subclass != 0x01U ||
        (device->prog_if & 0x80U) == 0U ||
        (device->prog_if & 0x01U) != 0U ||
        device->bars[4].type != PCI_BAR_IO ||
        (device->bars[4].flags & 1U) == 0U ||
        device->bars[4].base_low < 0x0100U ||
        device->bars[4].base_low > 0xFFF0U ||
        (device->bars[4].base_low & 0x0FU) != 0U) {
        return 0;
    }
    for (index = 0U; index < device_count; index++) {
        if (device == &devices[index]) {
            break;
        }
    }
    if (index == device_count) {
        return 0;
    }

    command = pci_config_read_word(device->bus, device->slot,
        device->function, 0x04U);
    command |= 0x0005U; /* I/O space and bus mastering only. */
    pci_config_write_word(device->bus, device->slot,
        device->function, 0x04U, command);
    command = pci_config_read_word(device->bus, device->slot,
        device->function, 0x04U);
    return (command & 0x0005U) == 0x0005U;
}

int pci_disable_ide_bus_master(const struct pci_device *device)
{
    unsigned int index;
    uint16_t command;

    if (device == 0) {
        return 0;
    }
    for (index = 0U; index < device_count; index++) {
        if (device == &devices[index]) {
            break;
        }
    }
    if (index == device_count) {
        return 0;
    }
    command = pci_config_read_word(device->bus, device->slot,
        device->function, 0x04U);
    command &= (uint16_t)~0x0004U;
    pci_config_write_word(device->bus, device->slot,
        device->function, 0x04U, command);
    command = pci_config_read_word(device->bus, device->slot,
        device->function, 0x04U);
    return (command & 0x0004U) == 0U;
}

unsigned int pci_ahci_controller_count(void)
{
    unsigned int count = 0U;
    unsigned int index;

    for (index = 0U; index < device_count; index++) {
        const struct pci_device *device = &devices[index];

        if ((device->header_type & 0x7FU) == 0U &&
            device->class_code == 0x01U && device->subclass == 0x06U &&
            device->prog_if == 0x01U) {
            count++;
        }
    }
    return count;
}

unsigned int pci_ahci_usable_count(void)
{
    unsigned int count = 0U;
    unsigned int index;

    for (index = 0U; index < device_count; index++) {
        uint32_t base;

        if (pci_ahci_mmio_base(&devices[index], &base)) {
            count++;
        }
    }
    return count;
}

const struct pci_device *pci_ahci_device_at(unsigned int requested_index)
{
    unsigned int found = 0U;
    unsigned int index;

    for (index = 0U; index < device_count; index++) {
        uint32_t base;

        if (pci_ahci_mmio_base(&devices[index], &base)) {
            if (found == requested_index) {
                return &devices[index];
            }
            found++;
        }
    }
    return 0;
}

int pci_prepare_ahci_controller(const struct pci_device *device,
    uint16_t *original_command)
{
    uint32_t base;
    uint16_t command;
    uint16_t prepared;

    if (original_command == 0 || !pci_device_is_recorded(device) ||
        !pci_ahci_mmio_base(device, &base)) {
        return 0;
    }
    command = pci_config_read_word(device->bus, device->slot,
        device->function, 0x04U);
    *original_command = command;
    prepared = (uint16_t)((command | 0x0002U) & (uint16_t)~0x0004U);
    pci_config_write_word(device->bus, device->slot, device->function,
        0x04U, prepared);
    if ((pci_config_read_word(device->bus, device->slot, device->function,
            0x04U) & 0x0006U) != 0x0002U) {
        command = pci_config_read_word(device->bus, device->slot,
            device->function, 0x04U);
        command &= (uint16_t)~0x0004U;
        pci_config_write_word(device->bus, device->slot, device->function,
            0x04U, command);
        return 0;
    }
    return 1;
}

int pci_enable_ahci_bus_master(const struct pci_device *device)
{
    uint32_t base;
    uint16_t command;

    if (!pci_device_is_recorded(device) ||
        !pci_ahci_mmio_base(device, &base)) {
        return 0;
    }
    command = pci_config_read_word(device->bus, device->slot,
        device->function, 0x04U);
    command |= 0x0006U;
    pci_config_write_word(device->bus, device->slot, device->function,
        0x04U, command);
    return (pci_config_read_word(device->bus, device->slot,
        device->function, 0x04U) & 0x0006U) == 0x0006U;
}

int pci_enable_ahci_msi(const struct pci_device *device,
    unsigned int apic_id, unsigned int vector)
{
    uint8_t configuration[256];
    struct pci_msi_layout layout;
    struct pci_msi_device_context context;
    struct pci_msi_register_io io;
    uint32_t address;
    uint16_t data;
    uint32_t base;
    unsigned int flags;
    int result;

    flags = interrupt_save();
    if (ahci_msi_state.valid != 0U || !pci_device_is_recorded(device) ||
        !pci_ahci_mmio_base(device, &base) ||
        !pci_msi_xapic_address(apic_id, &address) ||
        !pci_msi_fixed_data(vector, &data)) {
        interrupt_restore(flags);
        return 0;
    }
    pci_read_configuration(device, configuration);
    if (pci_msi_parse_layout(configuration, sizeof(configuration), &layout) !=
            PCI_MSI_PARSE_FOUND) {
        interrupt_restore(flags);
        return 0;
    }

    context.bus = device->bus;
    context.slot = device->slot;
    context.function = device->function;
    io = pci_msi_io(&context);
    result = pci_msi_program(&layout, &io, address, data,
        &ahci_msi_state);
    if (ahci_msi_state.valid != 0U) {
        ahci_msi_bus = device->bus;
        ahci_msi_slot = device->slot;
        ahci_msi_function = device->function;
    }
    interrupt_restore(flags);
    return result;
}

int pci_disable_ahci_msi(const struct pci_device *device)
{
    struct pci_msi_device_context context;
    struct pci_msi_register_io io;
    unsigned int flags;
    int result;

    flags = interrupt_save();
    if (!pci_device_is_recorded(device)) {
        interrupt_restore(flags);
        return 0;
    }
    if (ahci_msi_state.valid == 0U) {
        interrupt_restore(flags);
        return 1;
    }
    if (device->bus != ahci_msi_bus || device->slot != ahci_msi_slot ||
        device->function != ahci_msi_function) {
        interrupt_restore(flags);
        return 0;
    }
    context.bus = device->bus;
    context.slot = device->slot;
    context.function = device->function;
    io = pci_msi_io(&context);
    result = pci_msi_restore(&io, &ahci_msi_state);
    interrupt_restore(flags);
    return result;
}

int pci_quiesce_ahci_controller(const struct pci_device *device,
    uint16_t original_command)
{
    uint16_t command;
    uint16_t quiesced;
    int msi_quiesced;

    if (!pci_device_is_recorded(device)) {
        return 0;
    }
    msi_quiesced = pci_disable_ahci_msi(device);
    command = pci_config_read_word(device->bus, device->slot,
        device->function, 0x04U);
    command &= (uint16_t)~0x0004U;
    pci_config_write_word(device->bus, device->slot, device->function,
        0x04U, command);
    if ((pci_config_read_word(device->bus, device->slot,
            device->function, 0x04U) & 0x0004U) != 0U) {
        return 0;
    }

    quiesced = (uint16_t)(original_command & (uint16_t)~0x0004U);
    pci_config_write_word(device->bus, device->slot, device->function,
        0x04U, quiesced);
    command = pci_config_read_word(device->bus, device->slot,
        device->function, 0x04U);
    return msi_quiesced &&
        (command & 0x0007U) == (quiesced & 0x0007U);
}

unsigned int pci_device_count(void)
{
    return device_count;
}

const struct pci_device *pci_device_at(unsigned int index)
{
    if (index >= device_count) {
        return 0;
    }
    return &devices[index];
}
