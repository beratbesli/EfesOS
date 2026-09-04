#include "pci_msi.h"

#define PCI_CONFIGURATION_BYTES 256U
#define PCI_STATUS_OFFSET 0x06U
#define PCI_STATUS_CAPABILITIES 0x0010U
#define PCI_CAPABILITY_POINTER_OFFSET 0x34U
#define PCI_CAPABILITY_MINIMUM_OFFSET 0x40U
#define PCI_CAPABILITY_MAXIMUM_OFFSET 0xFCU
#define PCI_CAPABILITY_MSI 0x05U
#define PCI_CAPABILITY_MAXIMUM_NODES 48U
#define PCI_MSI_CONTROL_64_BIT 0x0080U
#define PCI_MSI_CONTROL_PER_VECTOR_MASKING 0x0100U
#define PCI_MSI_CONTROL_MMC_SHIFT 1U
#define PCI_MSI_CONTROL_MME_SHIFT 4U
#define PCI_MSI_CONTROL_COUNT_MASK 0x07U
#define PCI_XAPIC_MESSAGE_BASE 0xFEE00000U
#define PCI_X86_FIRST_DEVICE_VECTOR 0x20U
#define PCI_X86_LAST_DEVICE_VECTOR 0xFEU

static uint16_t read_u16(const uint8_t *bytes, unsigned int offset)
{
    return (uint16_t)((uint16_t)bytes[offset] |
        ((uint16_t)bytes[offset + 1U] << 8U));
}

static void clear_layout(struct pci_msi_layout *layout)
{
    uint8_t *bytes = (uint8_t *)layout;
    unsigned int index;

    for (index = 0U; index < sizeof(*layout); index++) {
        bytes[index] = 0U;
    }
}

static int valid_capability_pointer(unsigned int offset)
{
    return offset >= PCI_CAPABILITY_MINIMUM_OFFSET &&
        offset <= PCI_CAPABILITY_MAXIMUM_OFFSET &&
        (offset & 3U) == 0U;
}

static int parse_msi_capability(const uint8_t *configuration,
    unsigned int offset, struct pci_msi_layout *layout)
{
    uint16_t control = read_u16(configuration, offset + 2U);
    unsigned int capable =
        (control >> PCI_MSI_CONTROL_MMC_SHIFT) & PCI_MSI_CONTROL_COUNT_MASK;
    unsigned int enabled =
        (control >> PCI_MSI_CONTROL_MME_SHIFT) & PCI_MSI_CONTROL_COUNT_MASK;
    unsigned int is_64_bit = (control & PCI_MSI_CONTROL_64_BIT) != 0U;
    unsigned int per_vector =
        (control & PCI_MSI_CONTROL_PER_VECTOR_MASKING) != 0U;
    unsigned int data_offset = offset + (is_64_bit ? 12U : 8U);
    unsigned int mask_offset = data_offset + 4U;
    unsigned int pending_offset = mask_offset + 4U;
    unsigned int last_offset = per_vector ? pending_offset + 3U :
        data_offset + 1U;

    if (enabled > capable || last_offset >= PCI_CONFIGURATION_BYTES) {
        return PCI_MSI_PARSE_MALFORMED;
    }

    layout->capability_offset = (uint8_t)offset;
    layout->message_address_offset = (uint8_t)(offset + 4U);
    layout->message_upper_address_offset =
        is_64_bit ? (uint8_t)(offset + 8U) : 0U;
    layout->message_data_offset = (uint8_t)data_offset;
    layout->mask_bits_offset = per_vector ? (uint8_t)mask_offset : 0U;
    layout->pending_bits_offset =
        per_vector ? (uint8_t)pending_offset : 0U;
    layout->multiple_message_capable = (uint8_t)capable;
    layout->is_64_bit = (uint8_t)is_64_bit;
    layout->per_vector_masking = (uint8_t)per_vector;
    return PCI_MSI_PARSE_FOUND;
}

int pci_msi_parse_layout(const uint8_t *configuration,
    unsigned int configuration_length, struct pci_msi_layout *layout)
{
    uint32_t visited_low = 0U;
    uint32_t visited_high = 0U;
    unsigned int offset;
    unsigned int count;

    if (layout == 0) {
        return PCI_MSI_PARSE_MALFORMED;
    }
    clear_layout(layout);
    if (configuration == 0 ||
        configuration_length < PCI_CONFIGURATION_BYTES) {
        return PCI_MSI_PARSE_MALFORMED;
    }
    if ((read_u16(configuration, PCI_STATUS_OFFSET) &
            PCI_STATUS_CAPABILITIES) == 0U) {
        return PCI_MSI_PARSE_NOT_FOUND;
    }

    offset = configuration[PCI_CAPABILITY_POINTER_OFFSET];
    if (offset == 0U) {
        return PCI_MSI_PARSE_NOT_FOUND;
    }
    for (count = 0U; count < PCI_CAPABILITY_MAXIMUM_NODES; count++) {
        unsigned int next;
        unsigned int slot;
        uint32_t bit;
        uint32_t *visited;

        if (!valid_capability_pointer(offset)) {
            return PCI_MSI_PARSE_MALFORMED;
        }
        slot = offset >> 2U;
        visited = slot < 32U ? &visited_low : &visited_high;
        bit = 1U << (slot & 31U);
        if ((*visited & bit) != 0U) {
            return PCI_MSI_PARSE_MALFORMED;
        }
        *visited |= bit;

        if (configuration[offset] == PCI_CAPABILITY_MSI) {
            return parse_msi_capability(configuration, offset, layout);
        }
        next = configuration[offset + 1U];
        if (next == 0U) {
            return PCI_MSI_PARSE_NOT_FOUND;
        }
        offset = next;
    }
    return PCI_MSI_PARSE_MALFORMED;
}

int pci_msi_xapic_address(unsigned int apic_id, uint32_t *address)
{
    if (address == 0 || apic_id > 0xFFU) {
        return 0;
    }
    *address = PCI_XAPIC_MESSAGE_BASE | ((uint32_t)apic_id << 12U);
    return 1;
}

int pci_msi_fixed_data(unsigned int vector, uint16_t *data)
{
    if (data == 0 || vector < PCI_X86_FIRST_DEVICE_VECTOR ||
        vector > PCI_X86_LAST_DEVICE_VECTOR) {
        return 0;
    }
    *data = (uint16_t)vector;
    return 1;
}
