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

int pci_msi_parse_layout(const uint8_t *configuration,
    unsigned int configuration_length, struct pci_msi_layout *layout);
int pci_msi_xapic_address(unsigned int apic_id, uint32_t *address);
int pci_msi_fixed_data(unsigned int vector, uint16_t *data);

#endif
