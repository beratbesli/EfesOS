#ifndef EFESOS_HPET_TIME_H
#define EFESOS_HPET_TIME_H

typedef unsigned long long hpet_time_u64_t;

hpet_time_u64_t hpet_ticks_to_nanoseconds(hpet_time_u64_t ticks,
    unsigned int period_femtoseconds);
hpet_time_u64_t hpet_extend_counter32(unsigned int low,
    unsigned int *last_low, unsigned int *wrap_count);

#endif
