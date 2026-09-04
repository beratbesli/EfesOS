#include "pci_msi.h"

#define CONFIGURATION_BYTES 256U

static void set_u16(uint8_t *bytes, unsigned int offset, uint16_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1U] = (uint8_t)(value >> 8U);
}

static int layout_is_clear(const struct pci_msi_layout *layout)
{
    const uint8_t *bytes = (const uint8_t *)layout;
    unsigned int index;

    for (index = 0U; index < sizeof(*layout); index++) {
        if (bytes[index] != 0U) {
            return 0;
        }
    }
    return 1;
}

static int test_absent_and_invalid_inputs(void)
{
    uint8_t configuration[CONFIGURATION_BYTES] = {0};
    struct pci_msi_layout layout = {0};

    layout.capability_offset = 0xFFU;
    if (pci_msi_parse_layout(configuration, sizeof(configuration), &layout) !=
            PCI_MSI_PARSE_NOT_FOUND || !layout_is_clear(&layout)) {
        return 0;
    }
    set_u16(configuration, 0x06U, 0x0010U);
    if (pci_msi_parse_layout(configuration, sizeof(configuration), &layout) !=
            PCI_MSI_PARSE_NOT_FOUND ||
        pci_msi_parse_layout(0, sizeof(configuration), &layout) !=
            PCI_MSI_PARSE_MALFORMED ||
        pci_msi_parse_layout(configuration, 255U, &layout) !=
            PCI_MSI_PARSE_MALFORMED ||
        pci_msi_parse_layout(configuration, sizeof(configuration), 0) !=
            PCI_MSI_PARSE_MALFORMED) {
        return 0;
    }
    return 1;
}

static int test_32_bit_layout(void)
{
    uint8_t configuration[CONFIGURATION_BYTES] = {0};
    struct pci_msi_layout layout;

    set_u16(configuration, 0x06U, 0x0010U);
    configuration[0x34U] = 0x40U;
    configuration[0x40U] = 0x05U;
    set_u16(configuration, 0x42U, 0x0006U);
    if (pci_msi_parse_layout(configuration, sizeof(configuration), &layout) !=
            PCI_MSI_PARSE_FOUND) {
        return 0;
    }
    return layout.capability_offset == 0x40U &&
        layout.message_address_offset == 0x44U &&
        layout.message_upper_address_offset == 0U &&
        layout.message_data_offset == 0x48U &&
        layout.mask_bits_offset == 0U && layout.pending_bits_offset == 0U &&
        layout.multiple_message_capable == 3U && !layout.is_64_bit &&
        !layout.per_vector_masking;
}

static int test_64_bit_masked_layout(void)
{
    uint8_t configuration[CONFIGURATION_BYTES] = {0};
    struct pci_msi_layout layout;

    set_u16(configuration, 0x06U, 0x0010U);
    configuration[0x34U] = 0x40U;
    configuration[0x40U] = 0x01U;
    configuration[0x41U] = 0x50U;
    configuration[0x50U] = 0x05U;
    set_u16(configuration, 0x52U, 0x0184U);
    if (pci_msi_parse_layout(configuration, sizeof(configuration), &layout) !=
            PCI_MSI_PARSE_FOUND) {
        return 0;
    }
    return layout.capability_offset == 0x50U &&
        layout.message_address_offset == 0x54U &&
        layout.message_upper_address_offset == 0x58U &&
        layout.message_data_offset == 0x5CU &&
        layout.mask_bits_offset == 0x60U &&
        layout.pending_bits_offset == 0x64U &&
        layout.multiple_message_capable == 2U && layout.is_64_bit &&
        layout.per_vector_masking;
}

static int test_malformed_chains(void)
{
    uint8_t configuration[CONFIGURATION_BYTES] = {0};
    struct pci_msi_layout layout;

    set_u16(configuration, 0x06U, 0x0010U);
    configuration[0x34U] = 0x41U;
    if (pci_msi_parse_layout(configuration, sizeof(configuration), &layout) !=
            PCI_MSI_PARSE_MALFORMED) {
        return 0;
    }
    configuration[0x34U] = 0x3CU;
    if (pci_msi_parse_layout(configuration, sizeof(configuration), &layout) !=
            PCI_MSI_PARSE_MALFORMED) {
        return 0;
    }
    configuration[0x34U] = 0x40U;
    configuration[0x40U] = 0x01U;
    configuration[0x41U] = 0x44U;
    configuration[0x44U] = 0x09U;
    configuration[0x45U] = 0x40U;
    if (pci_msi_parse_layout(configuration, sizeof(configuration), &layout) !=
            PCI_MSI_PARSE_MALFORMED) {
        return 0;
    }
    configuration[0x45U] = 0xFFU;
    return pci_msi_parse_layout(configuration, sizeof(configuration), &layout) ==
        PCI_MSI_PARSE_MALFORMED;
}

static int test_malformed_msi(void)
{
    uint8_t configuration[CONFIGURATION_BYTES] = {0};
    struct pci_msi_layout layout;

    set_u16(configuration, 0x06U, 0x0010U);
    configuration[0x34U] = 0x40U;
    configuration[0x40U] = 0x05U;
    set_u16(configuration, 0x42U, 0x0020U);
    if (pci_msi_parse_layout(configuration, sizeof(configuration), &layout) !=
            PCI_MSI_PARSE_MALFORMED) {
        return 0;
    }
    configuration[0x34U] = 0xF8U;
    configuration[0xF8U] = 0x05U;
    set_u16(configuration, 0xFAU, 0x0180U);
    if (pci_msi_parse_layout(configuration, sizeof(configuration), &layout) !=
            PCI_MSI_PARSE_MALFORMED) {
        return 0;
    }
    configuration[0x34U] = 0xF4U;
    configuration[0xF4U] = 0x05U;
    set_u16(configuration, 0xF6U, 0U);
    return pci_msi_parse_layout(configuration, sizeof(configuration), &layout) ==
            PCI_MSI_PARSE_FOUND && layout.message_data_offset == 0xFCU;
}

static int test_message_encoding(void)
{
    uint32_t address = 0U;
    uint16_t data = 0U;

    return pci_msi_xapic_address(0U, &address) &&
        address == 0xFEE00000U &&
        pci_msi_xapic_address(255U, &address) &&
        address == 0xFEEFF000U &&
        !pci_msi_xapic_address(256U, &address) &&
        !pci_msi_xapic_address(0U, 0) &&
        pci_msi_fixed_data(0x20U, &data) && data == 0x20U &&
        pci_msi_fixed_data(0xFEU, &data) && data == 0xFEU &&
        !pci_msi_fixed_data(0x1FU, &data) &&
        !pci_msi_fixed_data(0xFFU, &data) &&
        !pci_msi_fixed_data(0x33U, 0);
}

int main(void)
{
    return test_absent_and_invalid_inputs() && test_32_bit_layout() &&
        test_64_bit_masked_layout() && test_malformed_chains() &&
        test_malformed_msi() && test_message_encoding() ? 0 : 1;
}
