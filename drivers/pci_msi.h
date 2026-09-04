#ifndef EFESOS_PCI_MSI_H
#define EFESOS_PCI_MSI_H

#include <stdint.h>

#define PCI_MSI_PARSE_MALFORMED (-1)
#define PCI_MSI_PARSE_NOT_FOUND 0
#define PCI_MSI_PARSE_FOUND 1

struct pci_msi_layout {
    uint8_t capability_offset;
    uint8_t message_address_offset;
    uint8_t message_upper_address_offset;
    uint8_t message_data_offset;
    uint8_t mask_bits_offset;
    uint8_t pending_bits_offset;
    uint8_t multiple_message_capable;
    uint8_t is_64_bit;
    uint8_t per_vector_masking;
};

struct pci_msi_register_io {
    void *context;
    uint16_t (*read_word)(void *context, uint8_t offset);
    uint32_t (*read_dword)(void *context, uint8_t offset);
    void (*write_word)(void *context, uint8_t offset, uint16_t value);
    void (*write_dword)(void *context, uint8_t offset, uint32_t value);
};

struct pci_msi_saved_state {
    struct pci_msi_layout layout;
    uint16_t control;
    uint32_t message_address;
    uint32_t message_upper_address;
    uint16_t message_data;
    uint32_t mask_bits;
    uint8_t valid;
};

int pci_msi_parse_layout(const uint8_t *configuration,
    unsigned int configuration_length, struct pci_msi_layout *layout);
int pci_msi_xapic_address(unsigned int apic_id, uint32_t *address);
int pci_msi_fixed_data(unsigned int vector, uint16_t *data);
int pci_msi_program(const struct pci_msi_layout *layout,
    const struct pci_msi_register_io *io, uint32_t message_address,
    uint16_t message_data, struct pci_msi_saved_state *state);
int pci_msi_restore(const struct pci_msi_register_io *io,
    struct pci_msi_saved_state *state);

#endif
