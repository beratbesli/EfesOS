#ifndef EFESOS_APIC_H
#define EFESOS_APIC_H

#include "acpi.h"

#define APIC_TIMER_VECTOR 0x32U
#define APIC_SPURIOUS_VECTOR 0xFFU

int apic_init(const struct acpi_madt_info *madt);
void apic_shutdown(void);
int apic_available(void);
int apic_enable_irq(unsigned int irq);
int apic_irq_enabled(unsigned int irq);
int apic_timer_init(void);
int apic_timer_available(void);
unsigned int apic_timer_initial_count(void);
void apic_acknowledge(void);
unsigned int apic_local_id(void);

#endif
