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
#define PCI_MSI_CONTROL_ENABLE 0x0001U
#define PCI_MSI_CONTROL_MME_MASK 0x0070U
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

static int valid_layout(const struct pci_msi_layout *layout)
{
    unsigned int expected_data;
    unsigned int expected_mask;

    if (layout == 0 ||
        !valid_capability_pointer(layout->capability_offset) ||
        layout->multiple_message_capable > PCI_MSI_CONTROL_COUNT_MASK ||
        layout->is_64_bit > 1U || layout->per_vector_masking > 1U ||
        layout->message_address_offset !=
            (uint8_t)(layout->capability_offset + 4U)) {
        return 0;
    }
    expected_data = layout->capability_offset +
        (layout->is_64_bit ? 12U : 8U);
    if (expected_data + 1U >= PCI_CONFIGURATION_BYTES ||
        layout->message_data_offset != expected_data ||
        layout->message_upper_address_offset !=
            (layout->is_64_bit ?
                (uint8_t)(layout->capability_offset + 8U) : 0U)) {
        return 0;
    }
    expected_mask = expected_data + 4U;
    if (!layout->per_vector_masking) {
        return layout->mask_bits_offset == 0U &&
            layout->pending_bits_offset == 0U;
    }
    return expected_mask + 7U < PCI_CONFIGURATION_BYTES &&
        layout->mask_bits_offset == expected_mask &&
        layout->pending_bits_offset == expected_mask + 4U;
}

static int valid_io(const struct pci_msi_register_io *io)
{
    return io != 0 && io->read_word != 0 && io->read_dword != 0 &&
        io->write_word != 0 && io->write_dword != 0;
}

static void clear_saved_state(struct pci_msi_saved_state *state)
{
    state->layout.capability_offset = 0U;
    state->layout.message_address_offset = 0U;
    state->layout.message_upper_address_offset = 0U;
    state->layout.message_data_offset = 0U;
    state->layout.mask_bits_offset = 0U;
    state->layout.pending_bits_offset = 0U;
    state->layout.multiple_message_capable = 0U;
    state->layout.is_64_bit = 0U;
    state->layout.per_vector_masking = 0U;
    state->control = 0U;
    state->message_address = 0U;
    state->message_upper_address = 0U;
    state->message_data = 0U;
    state->mask_bits = 0U;
    state->valid = 0U;
}

static int word_is(const struct pci_msi_register_io *io, uint8_t offset,
    uint16_t expected)
{
    return io->read_word(io->context, offset) == expected;
}

static int dword_is(const struct pci_msi_register_io *io, uint8_t offset,
    uint32_t expected)
{
    return io->read_dword(io->context, offset) == expected;
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

int pci_msi_restore(const struct pci_msi_register_io *io,
    struct pci_msi_saved_state *state)
{
    const struct pci_msi_layout *layout;
    uint16_t disabled_control;
    int restored = 1;

    if (!valid_io(io) || state == 0 || state->valid == 0U ||
        !valid_layout(&state->layout)) {
        return 0;
    }
    layout = &state->layout;
    disabled_control = (uint16_t)(io->read_word(io->context,
        (uint8_t)(layout->capability_offset + 2U)) &
        (uint16_t)~(PCI_MSI_CONTROL_ENABLE | PCI_MSI_CONTROL_MME_MASK));
    io->write_word(io->context,
        (uint8_t)(layout->capability_offset + 2U), disabled_control);
    if (!word_is(io, (uint8_t)(layout->capability_offset + 2U),
            disabled_control)) {
        restored = 0;
    }
    if (layout->per_vector_masking) {
        uint32_t masked = io->read_dword(io->context,
            layout->mask_bits_offset) | 1U;

        io->write_dword(io->context, layout->mask_bits_offset, masked);
        if (!dword_is(io, layout->mask_bits_offset, masked)) {
            restored = 0;
        }
    }

    io->write_dword(io->context, layout->message_address_offset,
        state->message_address);
    if (!dword_is(io, layout->message_address_offset,
            state->message_address)) {
        restored = 0;
    }
    if (layout->is_64_bit) {
        io->write_dword(io->context, layout->message_upper_address_offset,
            state->message_upper_address);
        if (!dword_is(io, layout->message_upper_address_offset,
                state->message_upper_address)) {
            restored = 0;
        }
    }
    io->write_word(io->context, layout->message_data_offset,
        state->message_data);
    if (!word_is(io, layout->message_data_offset, state->message_data)) {
        restored = 0;
    }
    if (layout->per_vector_masking) {
        io->write_dword(io->context, layout->mask_bits_offset,
            state->mask_bits);
        if (!dword_is(io, layout->mask_bits_offset, state->mask_bits)) {
            restored = 0;
        }
    }

    if (!restored) {
        return 0;
    }
    io->write_word(io->context,
        (uint8_t)(layout->capability_offset + 2U), state->control);
    if (!word_is(io, (uint8_t)(layout->capability_offset + 2U),
            state->control)) {
        return 0;
    }
    clear_saved_state(state);
    return 1;
}

int pci_msi_program(const struct pci_msi_layout *layout,
    const struct pci_msi_register_io *io, uint32_t message_address,
    uint16_t message_data, struct pci_msi_saved_state *state)
{
    uint16_t disabled_control;
    uint16_t enabled_control;
    uint32_t programmed_mask = 0U;
    int programmed = 1;

    if (!valid_layout(layout) || !valid_io(io) || state == 0 ||
        state->valid != 0U ||
        (message_address & 0xFFF00FFFU) != PCI_XAPIC_MESSAGE_BASE ||
        message_data < PCI_X86_FIRST_DEVICE_VECTOR ||
        message_data > PCI_X86_LAST_DEVICE_VECTOR) {
        return 0;
    }

    state->layout = *layout;
    state->control = io->read_word(io->context,
        (uint8_t)(layout->capability_offset + 2U));
    state->message_address = io->read_dword(io->context,
        layout->message_address_offset);
    state->message_upper_address = layout->is_64_bit ?
        io->read_dword(io->context,
            layout->message_upper_address_offset) : 0U;
    state->message_data = io->read_word(io->context,
        layout->message_data_offset);
    state->mask_bits = layout->per_vector_masking ?
        io->read_dword(io->context, layout->mask_bits_offset) : 0U;
    state->valid = 1U;

    if (((state->control & PCI_MSI_CONTROL_64_BIT) != 0U) !=
            (layout->is_64_bit != 0U) ||
        ((state->control & PCI_MSI_CONTROL_PER_VECTOR_MASKING) != 0U) !=
            (layout->per_vector_masking != 0U)) {
        clear_saved_state(state);
        return 0;
    }
    disabled_control = (uint16_t)(state->control &
        (uint16_t)~(PCI_MSI_CONTROL_ENABLE | PCI_MSI_CONTROL_MME_MASK));
    enabled_control = (uint16_t)(disabled_control | PCI_MSI_CONTROL_ENABLE);
    io->write_word(io->context,
        (uint8_t)(layout->capability_offset + 2U), disabled_control);
    if (!word_is(io, (uint8_t)(layout->capability_offset + 2U),
            disabled_control)) {
        programmed = 0;
    }
    if (programmed && layout->per_vector_masking) {
        programmed_mask = state->mask_bits | 1U;
        io->write_dword(io->context, layout->mask_bits_offset,
            programmed_mask);
        if (!dword_is(io, layout->mask_bits_offset, programmed_mask)) {
            programmed = 0;
        }
    }
    if (programmed) {
        io->write_dword(io->context, layout->message_address_offset,
            message_address);
        programmed = dword_is(io, layout->message_address_offset,
            message_address);
    }
    if (programmed && layout->is_64_bit) {
        io->write_dword(io->context,
            layout->message_upper_address_offset, 0U);
        programmed = dword_is(io,
            layout->message_upper_address_offset, 0U);
    }
    if (programmed) {
        io->write_word(io->context, layout->message_data_offset,
            message_data);
        programmed = word_is(io, layout->message_data_offset,
            message_data);
    }
    if (programmed) {
        io->write_word(io->context,
            (uint8_t)(layout->capability_offset + 2U), enabled_control);
        programmed = word_is(io,
            (uint8_t)(layout->capability_offset + 2U), enabled_control);
    }
    if (programmed && layout->per_vector_masking) {
        programmed_mask &= ~1U;
        io->write_dword(io->context, layout->mask_bits_offset,
            programmed_mask);
        programmed = dword_is(io, layout->mask_bits_offset,
            programmed_mask);
    }
    if (programmed) {
        return 1;
    }
    (void)pci_msi_restore(io, state);
    return 0;
}
