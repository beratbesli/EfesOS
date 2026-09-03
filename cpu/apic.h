#ifndef EFESOS_APIC_H
#define EFESOS_APIC_H

#include "acpi.h"

#define APIC_SPURIOUS_VECTOR 0xFFU

int apic_init(const struct acpi_madt_info *madt);
void apic_shutdown(void);
int apic_available(void);
int apic_enable_irq(unsigned int irq);
int apic_irq_enabled(unsigned int irq);
void apic_acknowledge(void);
unsigned int apic_local_id(void);

#endif
