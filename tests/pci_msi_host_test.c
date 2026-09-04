#include "pci_msi.h"

#define CONFIGURATION_BYTES 256U

struct mock_configuration {
    uint8_t bytes[CONFIGURATION_BYTES];
    unsigned int write_count;
    unsigned int fail_write;
};

static void set_u16(uint8_t *bytes, unsigned int offset, uint16_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1U] = (uint8_t)(value >> 8U);
}

static void set_u32(uint8_t *bytes, unsigned int offset, uint32_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1U] = (uint8_t)(value >> 8U);
    bytes[offset + 2U] = (uint8_t)(value >> 16U);
    bytes[offset + 3U] = (uint8_t)(value >> 24U);
}

static uint16_t mock_read_word(void *context, uint8_t offset)
{
    struct mock_configuration *configuration =
        (struct mock_configuration *)context;

    return (uint16_t)((uint16_t)configuration->bytes[offset] |
        ((uint16_t)configuration->bytes[(unsigned int)offset + 1U] << 8U));
}

static uint32_t mock_read_dword(void *context, uint8_t offset)
{
    struct mock_configuration *configuration =
        (struct mock_configuration *)context;

    return (uint32_t)configuration->bytes[offset] |
        ((uint32_t)configuration->bytes[(unsigned int)offset + 1U] << 8U) |
        ((uint32_t)configuration->bytes[(unsigned int)offset + 2U] << 16U) |
        ((uint32_t)configuration->bytes[(unsigned int)offset + 3U] << 24U);
}

static int mock_accepts_write(struct mock_configuration *configuration)
{
    configuration->write_count++;
    return configuration->write_count != configuration->fail_write;
}

static void mock_write_word(void *context, uint8_t offset, uint16_t value)
{
    struct mock_configuration *configuration =
        (struct mock_configuration *)context;

    if (mock_accepts_write(configuration)) {
        set_u16(configuration->bytes, offset, value);
    }
}

static void mock_write_dword(void *context, uint8_t offset, uint32_t value)
{
    struct mock_configuration *configuration =
        (struct mock_configuration *)context;

    if (mock_accepts_write(configuration)) {
        set_u32(configuration->bytes, offset, value);
    }
}

static struct pci_msi_register_io mock_io(
    struct mock_configuration *configuration)
{
    struct pci_msi_register_io io;

    io.context = configuration;
    io.read_word = mock_read_word;
    io.read_dword = mock_read_dword;
    io.write_word = mock_write_word;
    io.write_dword = mock_write_dword;
    return io;
}

static void prepare_masked_64_bit(struct mock_configuration *configuration,
    struct pci_msi_layout *layout)
{
    unsigned int index;

    for (index = 0U; index < sizeof(*configuration); index++) {
        ((uint8_t *)configuration)[index] = 0U;
    }
    set_u16(configuration->bytes, 0x06U, 0x0010U);
    configuration->bytes[0x34U] = 0x50U;
    configuration->bytes[0x50U] = 0x05U;
    set_u16(configuration->bytes, 0x52U, 0x0185U);
    set_u32(configuration->bytes, 0x54U, 0xABC00000U);
    set_u32(configuration->bytes, 0x58U, 0x12345678U);
    set_u16(configuration->bytes, 0x5CU, 0x0045U);
    set_u32(configuration->bytes, 0x60U, 0xA5A5A5A4U);
    (void)pci_msi_parse_layout(configuration->bytes,
        sizeof(configuration->bytes), layout);
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

static int test_program_and_restore(void)
{
    struct mock_configuration configuration;
    struct pci_msi_layout layout;
    struct pci_msi_saved_state state = {0};
    struct pci_msi_register_io io;

    prepare_masked_64_bit(&configuration, &layout);
    io = mock_io(&configuration);
    if (!pci_msi_program(&layout, &io, 0xFEE7F000U, 0x33U, &state) ||
        state.valid == 0U || mock_read_word(&configuration, 0x52U) !=
            0x0185U || mock_read_dword(&configuration, 0x54U) !=
            0xFEE7F000U || mock_read_dword(&configuration, 0x58U) != 0U ||
        mock_read_word(&configuration, 0x5CU) != 0x33U ||
        mock_read_dword(&configuration, 0x60U) != 0xA5A5A5A4U) {
        return 0;
    }
    return pci_msi_restore(&io, &state) && state.valid == 0U &&
        mock_read_word(&configuration, 0x52U) == 0x0185U &&
        mock_read_dword(&configuration, 0x54U) == 0xABC00000U &&
        mock_read_dword(&configuration, 0x58U) == 0x12345678U &&
        mock_read_word(&configuration, 0x5CU) == 0x0045U &&
        mock_read_dword(&configuration, 0x60U) == 0xA5A5A5A4U;
}

static int test_program_failure_rolls_back(void)
{
    unsigned int failed_write;

    for (failed_write = 1U; failed_write <= 7U; failed_write++) {
        struct mock_configuration configuration;
        struct pci_msi_layout layout;
        struct pci_msi_saved_state state = {0};
        struct pci_msi_register_io io;

        prepare_masked_64_bit(&configuration, &layout);
        configuration.fail_write = failed_write;
        io = mock_io(&configuration);
        if (pci_msi_program(&layout, &io, 0xFEE01000U, 0x33U, &state) ||
            state.valid != 0U ||
            mock_read_word(&configuration, 0x52U) != 0x0185U ||
            mock_read_dword(&configuration, 0x54U) != 0xABC00000U ||
            mock_read_dword(&configuration, 0x58U) != 0x12345678U ||
            mock_read_word(&configuration, 0x5CU) != 0x0045U ||
            mock_read_dword(&configuration, 0x60U) != 0xA5A5A5A4U) {
            return 0;
        }
    }
    return 1;
}

static int test_program_rejects_invalid_contracts(void)
{
    struct mock_configuration configuration;
    struct pci_msi_layout layout;
    struct pci_msi_saved_state state = {0};
    struct pci_msi_register_io io;

    prepare_masked_64_bit(&configuration, &layout);
    io = mock_io(&configuration);
    if (pci_msi_program(0, &io, 0xFEE00000U, 0x33U, &state) ||
        pci_msi_program(&layout, 0, 0xFEE00000U, 0x33U, &state) ||
        pci_msi_program(&layout, &io, 0xFED00000U, 0x33U, &state) ||
        pci_msi_program(&layout, &io, 0xFEE00001U, 0x33U, &state) ||
        pci_msi_program(&layout, &io, 0xFEE00000U, 0x1FU, &state) ||
        pci_msi_program(&layout, &io, 0xFEE00000U, 0xFFU, &state)) {
        return 0;
    }
    layout.message_data_offset++;
    return !pci_msi_program(&layout, &io, 0xFEE00000U, 0x33U, &state) &&
        !pci_msi_restore(&io, &state);
}

int main(void)
{
    if (!test_absent_and_invalid_inputs()) {
        return 1;
    }
    if (!test_32_bit_layout()) {
        return 2;
    }
    if (!test_64_bit_masked_layout()) {
        return 3;
    }
    if (!test_malformed_chains()) {
        return 4;
    }
    if (!test_malformed_msi()) {
        return 5;
    }
    if (!test_message_encoding()) {
        return 6;
    }
    if (!test_program_and_restore()) {
        return 7;
    }
    if (!test_program_failure_rolls_back()) {
        return 8;
    }
    return test_program_rejects_invalid_contracts() ? 0 : 9;
}
