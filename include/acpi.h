#ifndef EFESOS_ACPI_H
#define EFESOS_ACPI_H

#include "boot_info.h"

#define ACPI_MAX_IO_APICS 8U
#define ACPI_ISA_IRQ_COUNT 16U

struct acpi_hpet_info {
    unsigned int event_timer_block_id;
    unsigned int physical_address;
    unsigned int minimum_tick;
    unsigned char sequence_number;
    unsigned char page_protection;
};

struct acpi_io_apic_info {
    unsigned int id;
    unsigned int physical_address;
    unsigned int global_interrupt_base;
};

struct acpi_irq_override_info {
    unsigned int global_interrupt;
    unsigned int flags;
    unsigned char present;
};

struct acpi_madt_info {
    unsigned int local_apic_address;
    unsigned int flags;
    unsigned int enabled_local_apics;
    unsigned int enabled_x2apics;
    unsigned int local_apic_id_bitmap[8];
    unsigned int io_apic_count;
    struct acpi_io_apic_info io_apics[ACPI_MAX_IO_APICS];
    struct acpi_irq_override_info isa_overrides[ACPI_ISA_IRQ_COUNT];
};

int acpi_init(const struct boot_info *boot_info);
int acpi_available(void);
int acpi_hpet_available(void);
const struct acpi_hpet_info *acpi_hpet_get(void);
int acpi_madt_available(void);
const struct acpi_madt_info *acpi_madt_get(void);

#endif
