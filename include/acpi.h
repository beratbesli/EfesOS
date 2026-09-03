#ifndef EFESOS_ACPI_H
#define EFESOS_ACPI_H

#include "boot_info.h"

struct acpi_hpet_info {
    unsigned int event_timer_block_id;
    unsigned int physical_address;
    unsigned int minimum_tick;
    unsigned char sequence_number;
    unsigned char page_protection;
};

int acpi_init(const struct boot_info *boot_info);
int acpi_available(void);
int acpi_hpet_available(void);
const struct acpi_hpet_info *acpi_hpet_get(void);

#endif
