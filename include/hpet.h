#ifndef EFESOS_HPET_H
#define EFESOS_HPET_H

#include "acpi.h"

typedef unsigned long long hpet_tick_t;

int hpet_init(const struct acpi_hpet_info *firmware_info);
int hpet_available(void);
int hpet_self_test(void);
unsigned int hpet_period_femtoseconds(void);
int hpet_counter_is_64bit(void);
hpet_tick_t hpet_ticks(void);
hpet_tick_t hpet_nanoseconds(void);
void hpet_maintain(void);

#endif
